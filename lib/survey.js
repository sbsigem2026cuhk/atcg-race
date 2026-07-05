// Paste your Google Apps Script web app URL here after setup (see google-apps-script/SETUP.md)
const GOOGLE_SHEETS_URL = "https://script.google.com/macros/s/AKfycbxzQ9CcztCtIgR5x4U5yqosfWUyQwiONpJXsAfvM2SquYIy4nXXfqFywwCxP-IUhUA3/exec";

const SURVEY_SKIP_VALUE = "SKIP";

const PRE_QUESTIONS = [
  {
    id: "preQ1",
    en: "Before playing this game, did you know how the 4 alphabet letters (A, T, C, G) in DNA pair up with each other?",
    zh: "在玩這個遊戲之前，你知道 DNA 裡面的 4 個英文字母（A、T、C、G）是怎麼互相配對的嗎？",
    options: [
      { value: "A", en: "Yes, I know exactly how they pair.", zh: "知道，我很清楚它們怎麼配。" },
      { value: "B", en: "I know they pair up, but I forget who pairs with whom.", zh: "知道會配對，但忘了誰跟誰配。" },
      { value: "C", en: "I have no idea.", zh: "完全不知道。" },
    ],
  },
  {
    id: "preQ2",
    en: 'If you see a DNA letter "A", which letter do you think is its natural, matching partner?',
    zh: "憑你的印象或直覺，如果畫面上出現一個 DNA 字母 「A」，哪一個字母才是它命中注定的互補配對夥伴？",
    options: [
      { value: "A", en: "C", zh: "C" },
      { value: "B", en: "T", zh: "T" },
      { value: "C", en: "G", zh: "G" },
      { value: "D", en: "I don't know (IDK)", zh: "我不知道（IDK）" },
    ],
  },
];

const POST_QUESTIONS = [
  {
    id: "postQ1",
    en: 'Now that you\'ve completed the fast-paced challenge, what is the correct complementary sequence for the DNA strand "C - A - T - G"?',
    zh: "經歷了刺激的限時對戰後，請問 DNA 序列 「C - A - T - G」 的正確互補配對序列應該是？",
    options: [
      { value: "A", en: "G - T - A - C", zh: "G - T - A - C" },
      { value: "B", en: "G - A - T - C", zh: "G - A - T - C" },
      { value: "C", en: "C - A - T - G", zh: "C - A - T - G" },
    ],
  },
  {
    id: "postQ2",
    en: 'Which of the following pairs is a "Mutation / Mismatch" (ERROR)?',
    zh: "請問下列哪一組配對屬於「突變 / 錯誤配對（Mismatch）」？",
    options: [
      { value: "A", en: "A - T", zh: "A - T" },
      { value: "B", en: "C - G", zh: "C - G" },
      { value: "C", en: "G - T", zh: "G - T" },
      { value: "D", en: "T - A", zh: "T - A" },
    ],
  },
];

function renderSurveyQuestions(container, questions) {
  if (!container) return;
  container.innerHTML = "";

  questions.forEach((question, index) => {
    const block = document.createElement("fieldset");
    block.className = "survey-question";
    block.dataset.questionId = question.id;

    const legend = document.createElement("legend");
    legend.className = "survey-legend";
    legend.innerHTML =
      `<span class="survey-q-num">Q${index + 1}</span>` +
      `<span class="survey-q-en">${question.en}</span>` +
      `<span class="survey-q-zh">${question.zh}</span>`;
    block.appendChild(legend);

    question.options.forEach((option) => {
      const label = document.createElement("label");
      label.className = "survey-option";

      const input = document.createElement("input");
      input.type = "radio";
      input.name = question.id;
      input.value = option.value;
      input.required = true;

      const text = document.createElement("span");
      text.className = "survey-option-text";
      text.innerHTML =
        `<span class="survey-opt-en">${option.en}</span>` +
        `<span class="survey-opt-zh">${option.zh}</span>`;

      label.appendChild(input);
      label.appendChild(text);
      block.appendChild(label);
    });

    container.appendChild(block);
  });
}

function readSurveyAnswers(questions) {
  const answers = {};
  let complete = true;

  questions.forEach((question) => {
    const selected = document.querySelector(`input[name="${question.id}"]:checked`);
    if (!selected) {
      complete = false;
      answers[question.id] = "";
    } else {
      answers[question.id] = selected.value;
    }
  });

  return { answers, complete };
}

function createSurveySessionId() {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`;
}

async function submitSurveyResponse(payload) {
  if (!GOOGLE_SHEETS_URL) {
    console.warn("GOOGLE_SHEETS_URL is not set — survey data was not sent to Google Sheets.");
    return { ok: false, skipped: true };
  }

  try {
    await fetch(GOOGLE_SHEETS_URL, {
      method: "POST",
      mode: "no-cors",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return { ok: true };
  } catch (err) {
    console.error("Survey submit failed:", err);
    return { ok: false, error: err };
  }
}

function initSurveyUI() {
  renderSurveyQuestions(document.getElementById("pretest-questions"), PRE_QUESTIONS);
  renderSurveyQuestions(document.getElementById("posttest-questions"), POST_QUESTIONS);
}

initSurveyUI();
