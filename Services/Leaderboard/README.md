# Forbidden Code: Survivor leaderboard service

Public Sites Worker and D1 database for the browser leaderboard. See [the architecture, score rules, tests and deployment notes](../../Docs/Leaderboard.md).

Run `node --test tests/*.test.mjs` with Node 24. Source migrations are generated with the pinned Drizzle tools; deployed SQL is included in `drizzle/`. No package installation is needed to run Worker SQL tests.
