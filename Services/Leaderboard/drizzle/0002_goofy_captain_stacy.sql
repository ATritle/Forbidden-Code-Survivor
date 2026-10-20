ALTER TABLE `runs` ADD `edition` text DEFAULT 'browser' NOT NULL;--> statement-breakpoint
ALTER TABLE `scores` ADD `edition` text DEFAULT 'browser' NOT NULL;--> statement-breakpoint
CREATE INDEX `scores_edition_ranking` ON `scores` (`edition`,`score`,`recorded_at`);--> statement-breakpoint
CREATE INDEX `scores_edition_player` ON `scores` (`edition`,`player_hash`,`score`);