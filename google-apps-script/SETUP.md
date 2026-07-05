# Google Sheets setup for ATCG Race survey data

Follow these steps once. After that, every student who plays will send one row to your spreadsheet.

## 1. Create a Google Sheet

1. Go to [Google Sheets](https://sheets.google.com) and create a new spreadsheet.
2. Name it e.g. **ATCG Race — Survey Responses**.

## 2. Add the script

1. In the sheet: **Extensions → Apps Script**.
2. Delete any default code and paste the contents of `Code.gs` from this folder.
3. Save the project (name it e.g. **ATCG Race Survey**).
4. Run **`setupSheet`** once from the editor (Run ▶). Approve permissions when asked.
5. Check your sheet — you should see a **Responses** tab with column headers.

## 3. Deploy as web app

1. **Deploy → New deployment**.
2. Type: **Web app**.
3. Execute as: **Me**.
4. Who has access: **Anyone** (required so students can submit without signing in).
5. Click **Deploy** and copy the **Web app URL**.

## 4. Connect the game

1. Open `lib/survey.js` in this repo.
2. Paste your URL into:

   ```javascript
   const GOOGLE_SHEETS_URL = "https://script.google.com/macros/s/....../exec";
   ```

3. Commit and push (or test locally first).

## 5. Test

1. Open `dna-arduino.html`, complete pre-test, play a round, complete post-test, submit.
2. Refresh your Google Sheet — a new row should appear.

## Column reference

| Column | Content |
|--------|---------|
| timestamp | When post-test was submitted |
| sessionId | Links pre + post for one play session |
| name | Player display name |
| preQ1–preQ2 | Pre-test answers (A/B/C or A/B/C/D) |
| postQ1–postQ2 | Post-test answers |
| roundTimeSeconds | Time to complete the 20-base complement |
| attemptNumber | Which try (1 = first attempt this session) |
| errorCount | Wrong key presses during the run |
| avgDecisionMs | Average ms between correct keystrokes |
| won | `true` if they finished the sequence |

## Notes

- Opening the web app URL in a browser may show `doGet not found` — that is normal (POST only).
- Skipped answers are recorded as `SKIP`.
- Re-deploy the web app after editing `Code.gs`.
