CREATE TABLE `runs` (
	`id` text PRIMARY KEY NOT NULL,
	`player_hash` text NOT NULL,
	`token_hash` text NOT NULL,
	`created_at` integer NOT NULL,
	`expires_at` integer NOT NULL,
	`submitted_at` integer,
	`username` text,
	`score` integer,
	`kills` integer,
	`level` integer,
	`seconds` integer,
	`challenges` integer,
	`won` integer
);
--> statement-breakpoint
CREATE INDEX `runs_player_created` ON `runs` (`player_hash`,`created_at`);--> statement-breakpoint
CREATE INDEX `runs_expiry` ON `runs` (`expires_at`);--> statement-breakpoint
CREATE TABLE `scores` (
	`player_hash` text PRIMARY KEY NOT NULL,
	`id` text NOT NULL,
	`username` text NOT NULL,
	`score` integer NOT NULL,
	`kills` integer NOT NULL,
	`level` integer NOT NULL,
	`seconds` integer NOT NULL,
	`challenges` integer NOT NULL,
	`won` integer NOT NULL,
	`recorded_at` integer NOT NULL,
	`run_id` text NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `scores_id_unique` ON `scores` (`id`);--> statement-breakpoint
CREATE UNIQUE INDEX `scores_run_id_unique` ON `scores` (`run_id`);--> statement-breakpoint
CREATE INDEX `scores_ranking` ON `scores` (`score`,`recorded_at`);