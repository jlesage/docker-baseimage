// This file is part of the shadow-rs package.
//
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.
// spell-checker:ignore gpasswd gshadow nscd sysroot

//! `gpasswd` — administer `/etc/group` and `/etc/gshadow`.
//!
//! Drop-in replacement for GNU shadow-utils `gpasswd(1)`.

use std::fmt;
use std::io::{self, Write as _};
use std::path::Path;

use clap::{Arg, ArgAction, ArgGroup, Command};
use uucore::error::{UError, UResult};

use shadow_core::atomic;
use shadow_core::audit;
use shadow_core::crypt;
use shadow_core::group::{self, GroupEntry};
use shadow_core::gshadow::{self, GshadowEntry};
use shadow_core::lock::FileLock;
use shadow_core::login_defs::LoginDefs;
use shadow_core::nscd;
use shadow_core::passwd;
use shadow_core::sysroot::SysRoot;

mod options {
    pub const GROUP: &str = "GROUP";
    pub const ADD: &str = "add";
    pub const DELETE: &str = "delete";
    pub const ADMINISTRATORS: &str = "administrators";
    pub const MEMBERS: &str = "members";
    pub const REMOVE_PASSWORD: &str = "remove-password";
    pub const RESTRICT: &str = "restrict";
    pub const ROOT: &str = "root";
    pub const PREFIX: &str = "prefix";
}

mod exit_codes {
    pub const BAD_SYNTAX: i32 = 2;
    pub const BAD_ARGUMENT: i32 = 3;
    pub const GROUP_NOT_FOUND: i32 = 6;
    pub const CANT_UPDATE: i32 = 10;
}

#[derive(Debug)]
enum GpasswdError {
    BadSyntax(String),
    BadArgument(String),
    GroupNotFound(String),
    CantUpdate(String),
}

impl fmt::Display for GpasswdError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::BadSyntax(msg)
            | Self::BadArgument(msg)
            | Self::GroupNotFound(msg)
            | Self::CantUpdate(msg) => f.write_str(msg),
        }
    }
}

impl std::error::Error for GpasswdError {}

impl UError for GpasswdError {
    fn code(&self) -> i32 {
        match self {
            Self::BadSyntax(_) => exit_codes::BAD_SYNTAX,
            Self::BadArgument(_) => exit_codes::BAD_ARGUMENT,
            Self::GroupNotFound(_) => exit_codes::GROUP_NOT_FOUND,
            Self::CantUpdate(_) => exit_codes::CANT_UPDATE,
        }
    }
}

// ---------------------------------------------------------------------------
// Security hardening
// ---------------------------------------------------------------------------

// Hardening functions are now centralized in shadow_core::hardening.

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#[uucore::main]
#[allow(clippy::too_many_lines)]
pub fn uumain(args: impl uucore::Args) -> UResult<()> {
    let _clean_env = shadow_core::hardening::harden_process();

    let Some(matches) = shadow_core::cli::parse_args(uu_app(), args, |_| exit_codes::BAD_SYNTAX)?
    else {
        return Ok(());
    };

    if !shadow_core::hardening::caller_is_root() {
        uucore::show_error!("{}", shadow_core::os_error::permission_denied());
        return Err(shadow_core::cli::AlreadyPrinted(1).into());
    }

    let group_name = matches
        .get_one::<String>(options::GROUP)
        .ok_or_else(|| GpasswdError::BadSyntax("group name required".into()))?
        .clone();

    let add_user = matches.get_one::<String>(options::ADD).cloned();
    let del_user = matches.get_one::<String>(options::DELETE).cloned();
    let set_admins = matches.get_one::<String>(options::ADMINISTRATORS).cloned();
    let set_members = matches.get_one::<String>(options::MEMBERS).cloned();
    let remove_password = matches.get_flag(options::REMOVE_PASSWORD);
    let restrict = matches.get_flag(options::RESTRICT);

    let prefix = matches.get_one::<String>(options::PREFIX).map(Path::new);
    let root_dir = matches.get_one::<String>(options::ROOT).map(Path::new);
    let root = SysRoot::new(prefix.or(root_dir));

    // Except for -A and -M, the options cannot be combined (GNU gpasswd).
    let exclusive_count = u8::from(add_user.is_some())
        + u8::from(del_user.is_some())
        + u8::from(remove_password)
        + u8::from(restrict)
        + u8::from(set_admins.is_some() || set_members.is_some());
    if exclusive_count > 1 {
        return Err(GpasswdError::BadSyntax("invalid combination of options".into()).into());
    }

    // Interactive password change needs the hash before locks are held.
    let new_password_hash = if exclusive_count == 0 {
        Some(prompt_and_hash_password(&root)?)
    } else {
        None
    };

    // Block signals for the duration of the critical section so a SIGINT
    // between lock acquisition and atomic_write cannot leave stale lock files.
    let _signals = shadow_core::hardening::SignalBlocker::block_critical()
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot block signals: {e}")))?;

    // ------------------------------------------------------------------
    // Lock and update /etc/group
    // ------------------------------------------------------------------
    let group_path = root.group_path();
    let group_lock = FileLock::acquire(&group_path).map_err(|e| {
        GpasswdError::CantUpdate(format!("cannot lock {}: {e}", group_path.display()))
    })?;

    let mut group_entries = group::read_group_file(&group_path).map_err(|e| {
        GpasswdError::CantUpdate(format!("cannot read {}: {e}", group_path.display()))
    })?;

    let idx = group_entries
        .iter()
        .position(|g| g.name == group_name)
        .ok_or_else(|| {
            GpasswdError::GroupNotFound(format!("group '{group_name}' does not exist"))
        })?;

    // Validate usernames against /etc/passwd when required.
    let passwd_path = root.passwd_path();
    let passwd_entries = if passwd_path.exists() {
        passwd::read_passwd_file(&passwd_path).map_err(|e| {
            GpasswdError::CantUpdate(format!("cannot read {}: {e}", passwd_path.display()))
        })?
    } else {
        Vec::new()
    };
    let user_exists = |name: &str| passwd_entries.iter().any(|p| p.name == name);

    if let Some(ref user) = add_user {
        if !user_exists(user) {
            drop(group_lock);
            return Err(GpasswdError::BadArgument(format!("user '{user}' does not exist")).into());
        }
        add_member(&mut group_entries[idx], user);
    } else if let Some(ref user) = del_user {
        remove_member(&mut group_entries[idx], user);
    } else if let Some(ref list) = set_members {
        let users = parse_user_list(list);
        for user in &users {
            if !user_exists(user) {
                drop(group_lock);
                return Err(
                    GpasswdError::BadArgument(format!("user '{user}' does not exist")).into(),
                );
            }
        }
        group_entries[idx].members = users;
    } else if remove_password || restrict || new_password_hash.is_some() {
        // Group password lives in gshadow; keep the group field as 'x'.
        group_entries[idx].passwd = "x".to_string();
    }

    // -A only touches gshadow admins; group file is unchanged unless -M also set.
    if let Some(ref list) = set_admins {
        for user in parse_user_list(list) {
            if !user_exists(&user) {
                drop(group_lock);
                return Err(
                    GpasswdError::BadArgument(format!("user '{user}' does not exist")).into(),
                );
            }
        }
    }

    let modified_gid = group_entries[idx].gid;
    let members_for_gshadow = group_entries[idx].members.clone();

    atomic::atomic_write(&group_path, |f| group::write_group(&group_entries, f)).map_err(|e| {
        GpasswdError::CantUpdate(format!("cannot write {}: {e}", group_path.display()))
    })?;

    drop(group_lock);

    // ------------------------------------------------------------------
    // Update /etc/gshadow when present (same pattern as groupmod/groupdel)
    // ------------------------------------------------------------------
    let gshadow_path = root.gshadow_path();
    let needs_gshadow = add_user.is_some()
        || del_user.is_some()
        || set_admins.is_some()
        || set_members.is_some()
        || remove_password
        || restrict
        || new_password_hash.is_some();

    if gshadow_path.exists() && needs_gshadow {
        let gs_lock = FileLock::acquire(&gshadow_path).map_err(|e| {
            GpasswdError::CantUpdate(format!("cannot lock {}: {e}", gshadow_path.display()))
        })?;

        let mut gs_entries = gshadow::read_gshadow_file(&gshadow_path).map_err(|e| {
            GpasswdError::CantUpdate(format!("cannot read {}: {e}", gshadow_path.display()))
        })?;

        // Create a gshadow line if the group exists only in /etc/group.
        if !gs_entries.iter().any(|g| g.name == group_name) {
            gs_entries.push(GshadowEntry {
                name: group_name.clone(),
                passwd: "!".to_string(),
                admins: Vec::new(),
                members: members_for_gshadow,
            });
        }

        if let Some(gs) = gs_entries.iter_mut().find(|g| g.name == group_name) {
            if let Some(ref user) = add_user {
                add_member_gs(gs, user);
            }
            if let Some(ref user) = del_user {
                remove_member_gs(gs, user);
            }
            if let Some(ref list) = set_members {
                gs.members = parse_user_list(list);
            }
            if let Some(ref list) = set_admins {
                gs.admins = parse_user_list(list);
            }
            if remove_password {
                gs.passwd.clear();
            }
            if restrict {
                gs.passwd = "!".to_string();
            }
            if let Some(ref hash) = new_password_hash {
                gs.passwd.clone_from(hash);
            }
        }

        atomic::atomic_write(&gshadow_path, |f| gshadow::write_gshadow(&gs_entries, f)).map_err(
            |e| GpasswdError::CantUpdate(format!("cannot write {}: {e}", gshadow_path.display())),
        )?;

        drop(gs_lock);
    }

    nscd::invalidate_cache("group");

    audit::log_user_event("CHG_GROUP", &group_name, modified_gid, true);

    Ok(())
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn parse_user_list(s: &str) -> Vec<String> {
    if s.is_empty() {
        return Vec::new();
    }
    s.split(',')
        .map(str::trim)
        .filter(|u| !u.is_empty())
        .map(ToString::to_string)
        .collect()
}

fn add_member(entry: &mut GroupEntry, user: &str) {
    if !entry.members.iter().any(|m| m == user) {
        entry.members.push(user.to_string());
    }
}

fn remove_member(entry: &mut GroupEntry, user: &str) {
    entry.members.retain(|m| m != user);
}

fn add_member_gs(entry: &mut GshadowEntry, user: &str) {
    if !entry.members.iter().any(|m| m == user) {
        entry.members.push(user.to_string());
    }
}

fn remove_member_gs(entry: &mut GshadowEntry, user: &str) {
    entry.members.retain(|m| m != user);
    entry.admins.retain(|m| m != user);
}

fn prompt_and_hash_password(root: &SysRoot) -> Result<String, GpasswdError> {
    // Banner on stderr (visible even if /dev/tty prompts are preferred for input).
    eprintln!("Changing the password for group");
    let _ = io::stderr().flush();

    let pass1 = read_password("New Password: ")?;
    let pass2 = read_password("Re-enter new password: ")?;

    if *pass1 != *pass2 {
        return Err(GpasswdError::BadArgument("passwords do not match".into()));
    }
    if pass1.is_empty() {
        return Err(GpasswdError::BadArgument(
            "empty password not allowed".into(),
        ));
    }

    let defs = LoginDefs::load(&root.login_defs_path())
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot read login.defs: {e}")))?;
    let method = match defs.get("ENCRYPT_METHOD").unwrap_or("SHA512") {
        "SHA256" => crypt::CryptMethod::Sha256,
        "YESCRYPT" => crypt::CryptMethod::Yescrypt,
        _ => crypt::CryptMethod::Sha512,
    };
    crypt::hash_password(&pass1, method, None)
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot hash password: {e}")))
}

/// RAII guard that restores terminal echo on drop (same pattern as newgrp).
struct EchoGuard {
    tty: std::fs::File,
    old_termios: rustix::termios::Termios,
}

impl EchoGuard {
    /// Disable echo on the given tty file.
    fn disable(tty: std::fs::File) -> Result<Self, GpasswdError> {
        use std::os::unix::io::AsFd;

        let old_termios = rustix::termios::tcgetattr(tty.as_fd()).map_err(|e| {
            GpasswdError::CantUpdate(format!("cannot get terminal attributes: {e}"))
        })?;

        let mut new_termios = old_termios.clone();
        new_termios.local_modes &= !rustix::termios::LocalModes::ECHO;
        rustix::termios::tcsetattr(
            tty.as_fd(),
            rustix::termios::OptionalActions::Now,
            &new_termios,
        )
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot disable echo: {e}")))?;

        Ok(Self { tty, old_termios })
    }
}

impl Drop for EchoGuard {
    fn drop(&mut self) {
        use std::os::unix::io::AsFd;
        let _ = rustix::termios::tcsetattr(
            self.tty.as_fd(),
            rustix::termios::OptionalActions::Now,
            &self.old_termios,
        );
    }
}

/// Read a password from `/dev/tty` with echo disabled.
///
/// The returned password is wrapped in `Zeroizing` so it is scrubbed from
/// memory when dropped.
fn read_password(prompt: &str) -> Result<zeroize::Zeroizing<String>, GpasswdError> {
    use std::io::{BufRead, Write as _};

    let tty = std::fs::File::options()
        .read(true)
        .write(true)
        .open("/dev/tty")
        .map_err(|_| {
            GpasswdError::BadSyntax(
                "setting a group password requires a tty; use -a/-d/-A/-M/-r/-R non-interactively"
                    .into(),
            )
        })?;

    if !rustix::termios::isatty(&tty) {
        return Err(GpasswdError::BadSyntax(
            "setting a group password requires a tty; use -a/-d/-A/-M/-r/-R non-interactively"
                .into(),
        ));
    }

    (&tty)
        .write_all(prompt.as_bytes())
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot write prompt: {e}")))?;
    (&tty)
        .flush()
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot flush prompt: {e}")))?;

    // Clone the tty handle: one for the guard (to restore echo), one for reading.
    let tty_for_guard = tty
        .try_clone()
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot clone tty handle: {e}")))?;

    // Disable echo; restored automatically on drop.
    let guard = EchoGuard::disable(tty_for_guard)?;

    let mut buf = zeroize::Zeroizing::new(String::new());
    let mut reader = std::io::BufReader::new(&tty);
    reader
        .read_line(&mut buf)
        .map_err(|e| GpasswdError::CantUpdate(format!("cannot read password: {e}")))?;

    // Echo was off, so print a newline after the user presses Enter.
    drop(guard);
    let _ = (&tty).write_all(b"\n");

    Ok(zeroize::Zeroizing::new(
        buf.trim_end_matches(['\r', '\n']).to_string(),
    ))
}

#[must_use]
pub fn uu_app() -> Command {
    Command::new("gpasswd")
        .about("Administer group membership and the group password")
        .override_usage("gpasswd [options] group")
        .version(shadow_core::cli::VERSION)
        .after_help(shadow_core::cli::AFTER_HELP)
        .arg(
            Arg::new(options::ADD)
                .short('a')
                .long("add")
                .value_name("USER")
                .help("Add USER to the named group"),
        )
        .arg(
            Arg::new(options::DELETE)
                .short('d')
                .long("delete")
                .value_name("USER")
                .help("Remove USER from the named group"),
        )
        .arg(
            Arg::new(options::ADMINISTRATORS)
                .short('A')
                .long("administrators")
                .value_name("USER,...")
                .help("Set the list of administrative users"),
        )
        .arg(
            Arg::new(options::MEMBERS)
                .short('M')
                .long("members")
                .value_name("USER,...")
                .help("Set the list of group members"),
        )
        .arg(
            Arg::new(options::REMOVE_PASSWORD)
                .short('r')
                .long("remove-password")
                .help("Remove the password from the named group")
                .action(ArgAction::SetTrue),
        )
        .arg(
            Arg::new(options::RESTRICT)
                .short('R')
                .long("restrict")
                .help("Restrict access to the named group (password set to !)")
                .action(ArgAction::SetTrue),
        )
        .arg(
            // GNU gpasswd uses -Q for --root (-R is --restrict).
            Arg::new(options::ROOT)
                .short('Q')
                .long("root")
                .value_name("CHROOT_DIR")
                .help("Apply changes in the CHROOT_DIR directory"),
        )
        .arg(
            Arg::new(options::PREFIX)
                .short('P')
                .long("prefix")
                .value_name("PREFIX_DIR")
                .help("Directory prefix"),
        )
        .arg(
            Arg::new(options::GROUP)
                .required(true)
                .index(1)
                .help("Group to administer"),
        )
        .group(
            ArgGroup::new("exclusive")
                .args([
                    options::ADD,
                    options::DELETE,
                    options::REMOVE_PASSWORD,
                    options::RESTRICT,
                ])
                .multiple(false),
        )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_app_builds() {
        uu_app().debug_assert();
    }

    #[test]
    fn test_group_required() {
        assert!(uu_app().try_get_matches_from(["gpasswd"]).is_err());
    }

    #[test]
    fn test_add_flag() {
        let m = uu_app()
            .try_get_matches_from(["gpasswd", "-a", "alice", "devs"])
            .expect("valid args");
        assert_eq!(
            m.get_one::<String>(options::ADD).map(String::as_str),
            Some("alice")
        );
        assert_eq!(
            m.get_one::<String>(options::GROUP).map(String::as_str),
            Some("devs")
        );
    }

    #[test]
    fn test_delete_flag() {
        let m = uu_app()
            .try_get_matches_from(["gpasswd", "-d", "bob", "devs"])
            .expect("valid args");
        assert_eq!(
            m.get_one::<String>(options::DELETE).map(String::as_str),
            Some("bob")
        );
    }

    #[test]
    fn test_members_flag() {
        let m = uu_app()
            .try_get_matches_from(["gpasswd", "-M", "a,b", "devs"])
            .expect("valid args");
        assert_eq!(
            m.get_one::<String>(options::MEMBERS).map(String::as_str),
            Some("a,b")
        );
    }

    #[test]
    fn test_parse_user_list() {
        assert_eq!(
            parse_user_list("a,b,c"),
            vec!["a".to_string(), "b".to_string(), "c".to_string()]
        );
        assert!(parse_user_list("").is_empty());
    }

    fn skip_unless_root() -> bool {
        !rustix::process::geteuid().is_root()
    }

    #[test]
    fn test_add_user_integration() {
        if skip_unless_root() {
            return;
        }

        let dir = tempfile::tempdir().expect("tempdir");
        let etc = dir.path().join("etc");
        std::fs::create_dir_all(&etc).expect("etc");
        std::fs::write(etc.join("group"), "devs:x:1000:bob\n").expect("group");
        std::fs::write(etc.join("gshadow"), "devs:!::bob\n").expect("gshadow");
        std::fs::write(
            etc.join("passwd"),
            "bob:x:1000:1000::/home/bob:/bin/sh\nalice:x:1001:1001::/home/alice:/bin/sh\n",
        )
        .expect("passwd");

        let code = uumain(
            vec![
                "gpasswd".into(),
                "-a".into(),
                "alice".into(),
                "-P".into(),
                dir.path().as_os_str().to_owned(),
                "devs".into(),
            ]
            .into_iter(),
        );
        assert_eq!(code, 0);

        let group = std::fs::read_to_string(etc.join("group")).expect("read group");
        assert!(group.contains("alice"), "{group}");
        let gshadow = std::fs::read_to_string(etc.join("gshadow")).expect("read gshadow");
        assert!(gshadow.contains("alice"), "{gshadow}");
    }

    #[test]
    fn test_nonexistent_group_fails() {
        if skip_unless_root() {
            return;
        }

        let dir = tempfile::tempdir().expect("tempdir");
        let etc = dir.path().join("etc");
        std::fs::create_dir_all(&etc).expect("etc");
        std::fs::write(etc.join("group"), "root:x:0:\n").expect("group");

        let code = uumain(
            vec![
                "gpasswd".into(),
                "-a".into(),
                "alice".into(),
                "-P".into(),
                dir.path().as_os_str().to_owned(),
                "missing".into(),
            ]
            .into_iter(),
        );
        assert_ne!(code, 0);
    }
}
