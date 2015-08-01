/*
 * Copyright 2015 LastPass, Inc.
 * Shared folder support.
 */
#include "cmd.h"
#include "util.h"
#include "config.h"
#include "terminal.h"
#include "agent.h"
#include "kdf.h"
#include "endpoints.h"
#include "clipboard.h"
#include "upload-queue.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

struct share_args {
	struct session *session;
	struct blob *blob;
	enum blobsync sync;
	unsigned char key[KDF_HASH_LEN];
	const char *sharename;
	struct share *share;

	bool read_only;
	bool admin;
	bool hide_passwords;
};

#define share_userls_usage "userls SHARE"
#define share_useradd_usage "useradd [--read_only=[true|false] --hidden=[true|false] --admin=[true|false] SHARE USERNAME"

static char *checkmark(int x) {
	return (x) ? "x" : "_";
}

int share_userls(int argc, char **argv, struct share_args *args)
{
	UNUSED(argv);
	struct share_user *user;
	char name[40];
	LIST_HEAD(users);
	bool has_groups = false;

	if (argc)
		die_usage(cmd_share_usage);

	if (!args->share)
		die("Share %s not found.", args->sharename);

	if (lastpass_share_getinfo(args->session, args->share->id, &users))
		die("Unable to access user list for share %s\n",
		    args->sharename);

	terminal_printf(TERMINAL_FG_YELLOW TERMINAL_BOLD "%-40s %6s %6s %6s %6s %6s" TERMINAL_RESET "\n",
	       "User", "RO", "Admin", "Hide", "OutEnt", "Accept");

	list_for_each_entry(user, &users, list) {
		if (user->is_group) {
			has_groups = true;
			continue;
		}

		if (user->realname) {
			snprintf(name, sizeof(name), "%s <%s>",
				 user->realname, user->username);
		} else {
			snprintf(name, sizeof(name), "%s", user->username);
		}

		terminal_printf("%-40s %6s %6s %6s %6s %6s"
				"\n",
				name,
				checkmark(user->read_only),
				checkmark(user->admin),
				checkmark(user->hide_passwords),
				checkmark(user->outside_enterprise),
				checkmark(user->accepted));
	}

	if (!has_groups)
		return 0;


	terminal_printf(TERMINAL_FG_YELLOW TERMINAL_BOLD "%-40s %6s %6s %6s %6s %6s"
			TERMINAL_RESET "\n",
			"Group", "RO", "Admin", "Hide", "OutEnt", "Accept");

	list_for_each_entry(user, &users, list) {
		if (!user->is_group)
			continue;

		terminal_printf("%-40s %6s %6s %6s %6s %6s"
				"\n",
				user->username,
				checkmark(user->read_only),
				checkmark(user->admin),
				checkmark(user->hide_passwords),
				checkmark(user->outside_enterprise),
				checkmark(user->accepted));
	}
	return 0;
}

int share_useradd(int argc, char **argv, struct share_args *args)
{
	struct share_user new_user = {
		.read_only = args->read_only,
		.hide_passwords = args->hide_passwords,
		.admin = args->admin
	};

	if (argc != 1)
		die_usage(cmd_share_usage);

	new_user.username = argv[0];
	lastpass_share_user_add(args->session, args->share, &new_user);
	return 0;
}

#define SHARE_CMD(name) { #name, "share " share_##name##_usage, share_##name }
static struct {
	const char *name;
	const char *usage;
	int (*cmd)(int, char **, struct share_args *share);
} share_commands[] = {
	SHARE_CMD(userls),
	SHARE_CMD(useradd),
};
#undef SHARE_CMD

int cmd_share(int argc, char **argv)
{
	char *subcmd;

	static struct option long_options[] = {
		{"sync", required_argument, NULL, 'S'},
		{"color", required_argument, NULL, 'C'},
		{"read-only", required_argument, NULL, 'r'},
		{"hidden", required_argument, NULL, 'H'},
		{"admin", required_argument, NULL, 'a'},
		{0, 0, 0, 0}
	};

	struct share_args args = {
		.sync = BLOB_SYNC_AUTO,
		.read_only = true,
		.hide_passwords = true,
	};

	/*
	 * Parse out all option commands for all subcommands, and store
	 * them in the share_args struct.
	 *
	 * All commands have at least subcmd and sharename non-option args.
	 * Additional non-option commands are passed as argc/argv to the
	 * sub-command.
	 *
	 * Although we look up the share based on the supplied sharename,
	 * it may not exist, in the case of commands such as 'add'.  Subcmds
	 * should check args.share before using it.
	 */
	int option;
	int option_index;
	while ((option = getopt_long(argc, argv, "S:C:r:H:a:", long_options, &option_index)) != -1) {
		switch (option) {
			case 'S':
				args.sync = parse_sync_string(optarg);
				break;
			case 'C':
				terminal_set_color_mode(
					parse_color_mode_string(optarg));
				break;
			case 'r':
				args.read_only = parse_bool_arg_string(optarg);
				break;
			case 'H':
				args.hide_passwords =
					parse_bool_arg_string(optarg);
				break;
			case 'a':
				args.admin = parse_bool_arg_string(optarg);
				break;
			case '?':
			default:
				die_usage(cmd_share_usage);
		}
	}

	if (argc - optind < 2)
		die_usage(cmd_share_usage);

	subcmd = argv[optind++];
	args.sharename = argv[optind++];

	init_all(args.sync, args.key, &args.session, &args.blob);

	args.share = find_unique_share(args.blob, args.sharename);

	for (unsigned int i=0; i < ARRAY_SIZE(share_commands); i++) {
		if (strcmp(subcmd, share_commands[i].name) == 0) {
			share_commands[i].cmd(argc - optind, &argv[optind],
					      &args);
		}
	}

	session_free(args.session);
	blob_free(args.blob);
	return 0;
}
