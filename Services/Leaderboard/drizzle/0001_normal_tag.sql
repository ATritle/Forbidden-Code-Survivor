PRAGMA foreign_keys=OFF;--> statement-breakpoint
CREATE TABLE `__new_scores` (
	`player_hash` text NOT NULL,
	`id` text PRIMARY KEY NOT NULL,
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
INSERT INTO `__new_scores`("player_hash", "id", "username", "score", "kills", "level", "seconds", "challenges", "won", "recorded_at", "run_id") SELECT "player_hash", "id", "username", "score", "kills", "level", "seconds", "challenges", "won", "recorded_at", "run_id" FROM `scores`;--> statement-breakpoint
DROP TABLE `scores`;--> statement-breakpoint
ALTER TABLE `__new_scores` RENAME TO `scores`;--> statement-breakpoint
PRAGMA foreign_keys=ON;--> statement-breakpoint
CREATE UNIQUE INDEX `scores_run_id_unique` ON `scores` (`run_id`);--> statement-breakpoint
CREATE INDEX `scores_ranking` ON `scores` (`score`,`recorded_at`);--> statement-breakpoint
CREATE INDEX `scores_player_ranking` ON `scores` (`player_hash`,`score`,`recorded_at`);