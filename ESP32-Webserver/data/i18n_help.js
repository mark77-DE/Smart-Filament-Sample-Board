// i18n_help.js

let i18nData = {};
let helpData = {};
let currentLang = 'de';

// load language and help data

async function loadHelpAndLang(lang='en') {
    currentLang = lang;
    try {
        const res = await fetch(`/lang_${lang}.json`); // oder nur /help.json
        const data = await res.json();
        i18nData = data.i18n || {};
        helpData = data.help || {};
        applyTranslations();
        
    } catch(e) {
        console.error("Help/i18n JSON konnte nicht geladen werden", e);
    }
}


function applyTranslations(root = document) {
  root.querySelectorAll("[data-i18n]").forEach(el => {
    const key = el.dataset.i18n;
    if(i18nData[key]) el.textContent = i18nData[key];
  });
}


// show help overlay for a given key
function showHelp(key) {
    if(!helpData[key]) return;
    const overlay = document.getElementById('helpOverlay');
    const title = document.getElementById('helpTitle');     // ID statt Klasse
    const content = document.getElementById('helpContent'); // ID statt Klasse

    if (!title || !content) return; // Safety
    title.innerHTML = helpData[key].title;
    content.innerHTML = helpData[key].content;
    overlay.style.display = 'block';
}

// hide help overlay
function hideHelp() {
    const overlay = document.getElementById('helpOverlay');
    if (!overlay) return;
    overlay.style.display = 'none';
}

// Optional: language switcher setup
function setupLangSwitcher(selectId) {
    const sel = document.getElementById(selectId);
    if(!sel) return;
    sel.addEventListener('change', (e) => loadHelpAndLang(e.target.value));
}

// on language change:
document.getElementById('langSelect').addEventListener('change', async (e) => {
    const lang = e.target.value;
    await loadHelpAndLang(lang);
});


//injet help icons into cards with data-help attribute
function injectHelpIcons() {
    document.querySelectorAll('.card[data-help]').forEach(card => {
        const header = card.querySelector('.cardHeader');
        const helpId = card.dataset.help;

        if (!header || !helpId) return;

        const icon = document.createElement('span');
        icon.className = 'helpIcon';
        icon.innerHTML = '?';
        icon.addEventListener('click', () => showHelp(helpId));

        header.appendChild(icon);
    });
}



//make translation function available globally
window.t = function(key, params = {}) {
    let str = i18nData[key] || key;

    Object.keys(params).forEach(p => {
        str = str.replace(`{${p}}`, params[p]);
    });

    return str;
};



// Close-Button Event
function initHelpSystem() {
  const helpCloseBtn = document.getElementById('helpCloseBtn');
  const helpOverlay  = document.getElementById('helpOverlay');

  if (!helpCloseBtn || !helpOverlay) {
    return; // Seite hat kein Help-System
  }

  helpCloseBtn.addEventListener('click', hideHelp);

  helpOverlay.addEventListener('click', (e) => {
    if (e.target === helpOverlay) hideHelp();
  });
}

document.addEventListener("DOMContentLoaded", initHelpSystem);