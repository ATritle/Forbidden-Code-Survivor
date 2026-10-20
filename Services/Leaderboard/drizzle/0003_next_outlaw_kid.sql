CREATE TABLE `board_resets` (
	`id` text PRIMARY KEY NOT NULL,
	`edition` text NOT NULL,
	`reset_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE `score_archives` (
	`reset_id` text NOT NULL,
	`score_id` text NOT NULL,
	`payload` text NOT NULL,
	PRIMARY KEY(`reset_id`, `score_id`)
);
--> statement-breakpoint
ALTER TABLE `runs` ADD `rider_id` text;--> statement-breakpoint
ALTER TABLE `runs` ADD `loadout_json` text;--> statement-breakpoint
ALTER TABLE `scores` ADD `loadout_json` text;