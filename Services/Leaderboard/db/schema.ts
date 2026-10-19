import {sqliteTable,text,integer,index} from 'drizzle-orm/sqlite-core';
export const runs=sqliteTable('runs',{
 id:text('id').primaryKey(),playerHash:text('player_hash').notNull(),tokenHash:text('token_hash').notNull(),edition:text('edition').notNull().default('browser'),
 createdAt:integer('created_at').notNull(),expiresAt:integer('expires_at').notNull(),submittedAt:integer('submitted_at'),
 username:text('username'),score:integer('score'),kills:integer('kills'),level:integer('level'),seconds:integer('seconds'),challenges:integer('challenges'),won:integer('won')
},t=>[index('runs_player_created').on(t.playerHash,t.createdAt),index('runs_expiry').on(t.expiresAt)]);
export const scores=sqliteTable('scores',{
 playerHash:text('player_hash').notNull(),id:text('id').primaryKey(),username:text('username').notNull(),score:integer('score').notNull(),edition:text('edition').notNull().default('browser'),
 kills:integer('kills').notNull(),level:integer('level').notNull(),seconds:integer('seconds').notNull(),challenges:integer('challenges').notNull(),won:integer('won').notNull(),
 recordedAt:integer('recorded_at').notNull(),runId:text('run_id').notNull().unique()
},t=>[index('scores_ranking').on(t.score,t.recordedAt),index('scores_player_ranking').on(t.playerHash,t.score,t.recordedAt),index('scores_edition_ranking').on(t.edition,t.score,t.recordedAt),index('scores_edition_player').on(t.edition,t.playerHash,t.score)]);
