# Shared browser leaderboard — 0.2.4

The top navigation and gameplay HUD include **LEADERBOARD**. It pauses a run while open; Return or Escape resumes it. The end screen shows the final score and offers a username field, **SUBMIT HIGH SCORE**, and **SKIP & RETURN**. Ending a normal run from Settings also opens this screen. The last submitted username is remembered locally.

Scores are shared across players through a public Sites Worker and D1 database at https://forbidden-code-survivor-scores.brisk-cloud-6489.chatgpt.site. The game remains at https://atritle.github.io/Forbidden-Code-Survivor/. No GitHub or database credentials are included in browser code.

## Scoring and identity

Score = 100 × kills + 10 × full seconds survived + 500 × levels gained + 1,500 × contracts completed + 10,000 for defeating the boss. The HUD displays the current score; victory adds its bonus to the final result. The shared policy is `GameData/score-policy.mjs`, copied into `Web/src/` and `Services/Leaderboard/shared/`; tests reject drift.

The top ten entries show rank, username, score, kills, time, level and victory status. Each anonymous browser profile retains its three highest-scoring submitted runs. Each distinct run keeps its submitted username; changing the name does not bypass the three-score cap. The public board shows at most ten rows globally. A new qualifying run replaces that player’s lowest retained score. Equal scores use submission time, then score ID, for a stable order. Names are unverified display names and may repeat. Clearing browser storage creates a new anonymous identity; the old public high score is not deleted. This is not an account system or cross-device login.

Submitting explicitly publishes the username and run statistics. Skipping does not publish them. Usernames accept 3–20 ASCII letters, numbers, spaces, underscores, dots or hyphens after normalization. Browser rendering uses text nodes, not username HTML.

## Service behavior

- `GET /v1/leaderboard`: top ten public rows, without player identity hashes or run credentials.
- `POST /v1/runs`: register a normal run as it starts and return a random short-lived submission token. The anonymous browser key and submission token are hashed at rest.
- `POST /v1/scores`: validate the registered run, calculate points server-side, record the result and atomically retain its top three scores. Duplicate retries use the already-saved result.

Sessions expire after two hours. Requests have size limits, run registration is limited per anonymous identity, and checks reject impossible duration/outcome/count ranges. These are casual leaderboard checks: combat is browser-authoritative and can still be modified by a determined player. Strong competitive anti-cheat would require authoritative simulation or replay verification. No claim of cheat-proof scores is made.

The browser makes no score requests in the per-frame loop. Opening the board fetches its data; a normal run contacts the service only at registration and optional submission. An unavailable service does not block gameplay. A failed submission can retry; a run whose initial registration failed cannot later invent a server start time. Testing-range, QA and performance runs are excluded from public submission.

## Development and deployment

`Services/Leaderboard` has its own Sites source checkout. Its `.openai/hosting.json` contains the exact project ID and the `DB` binding. Drizzle owns schema-only migrations under `drizzle/`. The dependency-free production Worker is flattened into `dist/server/index.js`; Sites provisions D1 and applies migrations during deployment. Dev tooling is pinned in the pnpm lockfile. Do not edit applied migrations; append new ones.

For a local UI test, run `node Services/Leaderboard/scripts/dev.mjs` and `node Web/serve.mjs 4174`. Open `http://127.0.0.1:4174/?qa=1&scoreApi=local`. This explicitly routes scores to a separate local SQLite database on port 4175; the override is unavailable on the public hostname. No local test scores are copied into the public database.

Run `node --test Web/tests/*.test.mjs` and `node --test Services/Leaderboard/tests/*.test.mjs`. The second command uses Node 24 SQLite to exercise the actual Worker SQL: ranking, top-three replacement, global top-ten limits, preserving existing scores during migration, duplicate submissions, invalid credentials/timing/names, CORS, rate limits and expiry. UI checks also cover a submitted username surviving reload, and skip/test-run behavior.

Pages deploys frontend changes through GitHub Actions. Backend changes additionally require the Sites source-helper save/deploy workflow for project `appgprj_6ad55fd9a49c8191974850643f1c01a3`; updating the GitHub copy alone does not redeploy the score service. The current Windows installer does not submit to this browser leaderboard.
