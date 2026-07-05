/**
 * ATCG Race — Pre/Post Test → Google Sheets
 *
 * Setup: see SETUP.md in this folder.
 */

function doPost(e) {
  try {
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Responses");
    if (!sheet) {
      sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
    }

    var data = JSON.parse(e.postData.contents);

    sheet.appendRow([
      data.timestamp || new Date().toISOString(),
      data.sessionId || "",
      data.name || "Anonymous",
      data.preQ1 || "",
      data.preQ2 || "",
      data.postQ1 || "",
      data.postQ2 || "",
      data.roundTimeSeconds !== undefined ? data.roundTimeSeconds : "",
      data.attemptNumber !== undefined ? data.attemptNumber : "",
      data.errorCount !== undefined ? data.errorCount : "",
      data.avgDecisionMs !== undefined ? data.avgDecisionMs : "",
      data.won !== undefined ? data.won : "",
    ]);

    return ContentService.createTextOutput(JSON.stringify({ ok: true }))
      .setMimeType(ContentService.MimeType.JSON);
  } catch (err) {
    return ContentService.createTextOutput(JSON.stringify({ ok: false, error: String(err) }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

function setupSheet() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheet = ss.getSheetByName("Responses");
  if (!sheet) {
    sheet = ss.insertSheet("Responses");
  }
  sheet.clearContents();
  sheet.appendRow([
    "timestamp",
    "sessionId",
    "name",
    "preQ1",
    "preQ2",
    "postQ1",
    "postQ2",
    "roundTimeSeconds",
    "attemptNumber",
    "errorCount",
    "avgDecisionMs",
    "won",
  ]);
  sheet.setFrozenRows(1);
}
