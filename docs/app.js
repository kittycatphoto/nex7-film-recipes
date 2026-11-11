/* NEX-7 Film Recipes — local configurator and NXR3/UXC writers */
'use strict';

const BASE_STYLES = Object.freeze(['Standard', 'Vivid', 'Neutral', 'Portrait', 'Landscape', 'B/W']);
const CONFIG_PATH = 'hook/nexhook-recipes.bin';
const UXC_PATH = 'hook/string_english.uxc';
const BASE_BUNDLE_URL = 'downloads/nex7-base-installer.zip';
const UTF8_ENCODER = new TextEncoder();
const UTF8_DECODER = new TextDecoder('utf-8', { fatal: true });
const ZIP_MTIME = new Date(2026, 10, 11, 0, 0, 0);

/*
 * Values mirror scripts/recipe_config.py exactly. `title` is the full source
 * recipe name; `name` is its <=13-byte English camera label from the base bank.
 */
const DEFAULT_RECIPES = Object.freeze([
  {
    key: 'C9', style: 1, slot: 'Vivid', ids: [0x146], stock: 'Vivid',
    title: 'Ektachrome E100', name: 'Ektachrome100', film: 'Slide film',
    description: 'Cool precision, clean saturation, and a crisp slide-film palette.',
    enabled: true, base: 1, sat: 0, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 58, wbLb: -2, wbCc: 0,
    palette: ['#91aebc', '#d8c5a4', '#f3d6a0', '#6d7f82', '#344f54', '#486a72']
  },
  {
    key: 'C4', style: 2, slot: 'Neutral', ids: [0x63], stock: 'Neutral',
    title: 'Patina Pastel', name: 'Patina Pastel', film: 'Soft color',
    description: 'Gentle portrait color with a warm, softly faded patina.',
    enabled: true, base: 3, sat: -2, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 0, wbTemp: 0, wbLb: 3, wbCc: 0,
    palette: ['#c8b9ae', '#e2cbb5', '#f4d9b7', '#9a8b83', '#665f62', '#8d7373']
  },
  {
    key: 'C7', style: 3, slot: 'Portrait', ids: [0x147], stock: 'Portrait',
    title: 'Portra 400', name: 'Portra 400', film: 'Color negative',
    description: 'Warm skin tones and an easy, forgiving everyday palette.',
    enabled: true, base: 3, sat: 0, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 56, wbLb: 2, wbCc: 1,
    palette: ['#c6a78e', '#e4c7a2', '#f1d4a5', '#9c8069', '#4d5145', '#7b6756']
  },
  {
    key: 'C6', style: 4, slot: 'Landscape', ids: [0x148], stock: 'Landscape',
    title: 'Improved Velvia', name: 'Improv Velvia', film: 'Landscape slide',
    description: 'Crisp landscape color with a restrained, vivid edge.',
    enabled: true, base: 4, sat: 1, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 53, wbLb: -1, wbCc: 0,
    palette: ['#789eac', '#d2c49c', '#f1d28a', '#587b70', '#244b3d', '#395c51']
  },
  {
    key: 'C11', style: 5, slot: 'B/W', ids: [0x14a], stock: 'B/W',
    title: 'Acros + Yellow', name: 'Acros Yellow', film: 'Monochrome + Y',
    description: 'Monochrome with a yellow-filter lift for skies and foliage.',
    enabled: true, base: 5, sat: 0, hue: 0, ppLb: 3, ppCc: 0,
    wbMode: 0, wbTemp: 0, wbLb: 0, wbCc: 0,
    palette: ['#aaa9a2', '#d2cbb9', '#e6dfc6', '#777a73', '#313630', '#54584f']
  },
  {
    key: 'C3', style: 6, slot: 'Clear', ids: [0x64], stock: 'Clear',
    title: 'Kodak Gold 200', name: 'Kodak Gold200', film: 'Color negative',
    description: 'Golden highlights and an affectionate, unmistakably warm bias.',
    enabled: true, base: 0, sat: 2, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 58, wbLb: 3, wbCc: 1,
    palette: ['#d4a56f', '#e9c787', '#f7dc9b', '#a37b50', '#52523a', '#80633e']
  },
  {
    key: 'C5', style: 7, slot: 'Deep', ids: [0x65], stock: 'Deep',
    title: 'NPL 160 Tungsten', name: 'NPL160 Tungst', film: 'Tungsten film',
    description: 'Tungsten-balanced color for interiors and artificial light.',
    enabled: true, base: 0, sat: -1, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 40, wbLb: 0, wbCc: 1,
    palette: ['#668b9e', '#b49b89', '#e2b783', '#646878', '#2c354b', '#485268']
  },
  {
    key: 'C2', style: 8, slot: 'Light', ids: [0x66], stock: 'Light',
    title: 'Kanda Line', name: 'Kanda Line', film: 'Urban color',
    description: 'Understated saturation and a cooler, quiet urban palette.',
    enabled: true, base: 0, sat: -2, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 53, wbLb: -1, wbCc: 0,
    palette: ['#88a3ac', '#b7c0ba', '#d9d5b8', '#6d7e81', '#36494e', '#4d6267']
  },
  {
    key: 'C1', style: 9, slot: 'Sunset', ids: [0x149], stock: 'Sunset',
    title: 'Golden Cinema', name: 'Golden Cinema', film: 'Cinematic color',
    description: 'A warm, cinematic glow made for the last light of day.',
    enabled: true, base: 0, sat: 1, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 65, wbLb: 1, wbCc: 0,
    palette: ['#c57d5e', '#e9b77c', '#ffe0a1', '#8c6653', '#3e4438', '#6e4e3e']
  },
  {
    key: 'C8', style: 10, slot: 'Night', ids: [0x68, 0x6d], stock: 'Night',
    title: 'Cinestill 800T', name: 'Cinestill800T', film: 'Tungsten / night',
    description: 'Cool tungsten balance and saturated color for after dark.',
    enabled: true, base: 0, sat: 1, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 34, wbLb: 0, wbCc: 1,
    palette: ['#3b647c', '#9a6c6c', '#e5a277', '#494b68', '#1d293c', '#303f59']
  },
  {
    key: 'C10', style: 11, slot: 'Autumn', ids: [0x6e, 0x6a], stock: 'Autumn',
    title: 'Classic Neg / Superia', name: 'Classic Neg', film: 'Classic negative',
    description: 'Muted saturation with a cool, nostalgic green color bias.',
    enabled: true, base: 0, sat: -1, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 10, wbTemp: 53, wbLb: -1, wbCc: -2,
    palette: ['#8c9c91', '#c0b7a0', '#dbc49d', '#68796c', '#35473c', '#4e5c4b']
  },
  {
    key: 'C12', style: 13, slot: 'Sepia', ids: [0x70, 0x6b], stock: 'Sepia',
    title: 'Tri-X Push', name: 'Tri-X Push', film: 'Monochrome',
    description: 'A direct black-and-white starting point, ready for your exposure.',
    enabled: true, base: 5, sat: 0, hue: 0, ppLb: 0, ppCc: 0,
    wbMode: 0, wbTemp: 0, wbLb: 0, wbCc: 0,
    palette: ['#8e918d', '#bab7aa', '#ded7c3', '#686c67', '#242a27', '#444a45']
  }
]);

function createDefaultRecipes() {
  return DEFAULT_RECIPES.map(recipe => ({ ...recipe, ids: [...recipe.ids], palette: [...recipe.palette] }));
}

function utf8Length(text) {
  return UTF8_ENCODER.encode(String(text)).length;
}

function validateRecipes(recipes) {
  const errors = [];
  if (!Array.isArray(recipes) || recipes.length !== DEFAULT_RECIPES.length) {
    return [{ style: -1, key: '*', message: 'Exactly 12 recipe entries are required.' }];
  }

  const seenStyles = new Set();
  const ranges = {
    sat: [-8, 7], hue: [-7, 7], ppLb: [-9, 9], ppCc: [-9, 9],
    wbTemp: [0, 100], wbLb: [-9, 9], wbCc: [-9, 9]
  };
  const shortLabels = {
    sat: 'Saturation', hue: 'Hue', ppLb: 'PP LB', ppCc: 'PP CC',
    wbTemp: 'Temperature', wbLb: 'WB LB', wbCc: 'WB CC'
  };

  for (const recipe of recipes) {
    const style = recipe && Number.isInteger(recipe.style) ? recipe.style : -1;
    if (!recipe || !DEFAULT_RECIPES.some(item => item.style === style) || seenStyles.has(style)) {
      errors.push({ style, key: '*', message: 'Recipe style slots must be unique and recognized.' });
      continue;
    }
    seenStyles.add(style);

    if (typeof recipe.name !== 'string' || recipe.name.length === 0) {
      errors.push({ style, key: 'name', message: 'Enter a camera label.' });
    } else if (recipe.name.includes('\0')) {
      errors.push({ style, key: 'name', message: 'Camera labels cannot contain NUL.' });
    } else if (utf8Length(recipe.name) > 13) {
      errors.push({ style, key: 'name', message: 'Use at most 13 UTF-8 bytes.' });
    }

    if (!Number.isInteger(recipe.base) || recipe.base < 0 || recipe.base > 5) {
      errors.push({ style, key: 'base', message: 'Choose a base style from 0 to 5.' });
    }
    if (![0, 10, 0xff].includes(recipe.wbMode)) {
      errors.push({ style, key: 'wbMode', message: 'Choose Auto, C.Temp, or Preserve.' });
    }
    for (const [key, [min, max]] of Object.entries(ranges)) {
      if (!Number.isInteger(recipe[key]) || recipe[key] < min || recipe[key] > max) {
        errors.push({ style, key, message: `${shortLabels[key]} must be an integer from ${min} to ${max}.` });
      }
    }
  }
  return errors;
}

function assertValidRecipes(recipes) {
  const errors = validateRecipes(recipes);
  if (errors.length) {
    const first = errors[0];
    const where = first.style >= 0 ? `Style ${first.style}: ` : '';
    throw new Error(`${where}${first.message}`);
  }
}

function buildConfig(recipes) {
  assertValidRecipes(recipes);
  const byStyle = new Map(recipes.map(recipe => [recipe.style, recipe]));
  const output = new Uint8Array(4 + 16 * 10);
  output.set([0x4e, 0x58, 0x52, 0x33], 0); // NXR3

  for (let style = 0; style < 16; style += 1) {
    const recipe = byStyle.get(style);
    const enabled = Boolean(recipe && recipe.enabled);
    const values = enabled ? [
      style, recipe.base, recipe.sat, recipe.hue, recipe.ppLb, recipe.ppCc,
      recipe.wbMode, recipe.wbTemp, recipe.wbLb, recipe.wbCc
    ] : [style, style, 0, 0, 0, 0, 0xff, 0, 0, 0];
    output.set(values.map(value => value & 0xff), 4 + style * 10);
  }
  return output;
}

function asUint8Array(data) {
  if (data instanceof Uint8Array) return data;
  if (data instanceof ArrayBuffer) return new Uint8Array(data);
  if (ArrayBuffer.isView(data)) return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  throw new Error('Expected binary data.');
}

function parseUxc(data) {
  const original = asUint8Array(data);
  if (original.length < 16) throw new Error('The English UXC bank is truncated.');
  const view = new DataView(original.buffer, original.byteOffset, original.byteLength);
  if (original[0] !== 0x75 || original[1] !== 0x78 || original[2] !== 0x63 || original[3] !== 0x07) {
    throw new Error('Bad UXC magic; expected uxc version 7.');
  }
  if (view.getUint32(8, true) !== 8) throw new Error('Only flags-8 UXC string banks are supported.');

  const packed = view.getUint32(12, true);
  const count = packed & 0x0fff;
  const words = packed >>> 12;
  const base = 0x10 + 2 * words;
  if (!count || words < count || base > original.length || base % 4 !== 0 || 0x10 + count * 2 > base) {
    throw new Error('Invalid UXC offset table dimensions.');
  }

  const strings = [];
  for (let index = 0; index < count; index += 1) {
    const offset = view.getUint16(0x10 + index * 2, true);
    const start = base + offset * 4;
    if (start < base || start + 4 > original.length) throw new Error(`UXC record ${index} is out of bounds.`);
    if (view.getUint16(start, true) !== index || original[start + 2] !== 0x00 || original[start + 3] !== 0x1e) {
      throw new Error(`UXC record ${index} has an invalid ID or tag.`);
    }
    let end = start + 4;
    while (end < original.length && original[end] !== 0) end += 1;
    if (end === original.length) throw new Error(`UXC record ${index} has no NUL terminator.`);
    try {
      strings.push(UTF8_DECODER.decode(original.subarray(start + 4, end)));
    } catch (_error) {
      throw new Error(`UXC record ${index} contains invalid UTF-8.`);
    }
  }
  return { header: original.slice(0, base), strings, count, words, base };
}

function buildUxc(data, recipes) {
  assertValidRecipes(recipes);
  const original = asUint8Array(data);
  const parsed = parseUxc(original);
  const replacements = new Map();

  for (const recipe of recipes) {
    const label = recipe.enabled ? recipe.name : recipe.stock;
    for (const id of recipe.ids) {
      if (id >= parsed.count) throw new Error(`UXC is missing required style record 0x${id.toString(16)}.`);
      replacements.set(id, label);
    }
  }

  const header = parsed.header.slice();
  const headerView = new DataView(header.buffer, header.byteOffset, header.byteLength);
  const records = [];
  let recordsLength = 0;

  for (let index = 0; index < parsed.count; index += 1) {
    const text = replacements.has(index) ? replacements.get(index) : parsed.strings[index];
    const encoded = UTF8_ENCODER.encode(text);
    const recordLength = 4 + encoded.length + 1;
    const paddedLength = (recordLength + 3) & ~3;
    const relative = recordsLength;
    if (relative % 4 !== 0 || relative / 4 > 0xffff) {
      throw new Error(`UXC record ${index} exceeds the 16-bit offset table.`);
    }
    headerView.setUint16(0x10 + index * 2, relative / 4, true);

    const record = new Uint8Array(paddedLength);
    record[0] = index & 0xff;
    record[1] = index >>> 8;
    record[2] = 0x00;
    record[3] = 0x1e;
    record.set(encoded, 4);
    records.push(record);
    recordsLength += paddedLength;
  }

  const output = new Uint8Array(header.length + recordsLength);
  output.set(header, 0);
  let position = header.length;
  for (const record of records) {
    output.set(record, position);
    position += record.length;
  }
  return output;
}

function zipEntry(data, filename) {
  return [data, {
    level: 6,
    mtime: ZIP_MTIME,
    os: 3,
    attrs: (filename.endsWith('.sh') ? 0o755 : 0o644) << 16
  }];
}

function createCustomZip(baseFiles, recipes, fflateLibrary) {
  if (!fflateLibrary || typeof fflateLibrary.zipSync !== 'function') {
    throw new Error('The vendored fflate ZIP library is unavailable.');
  }
  if (!baseFiles || !baseFiles[CONFIG_PATH] || !baseFiles[UXC_PATH]) {
    throw new Error('The base ZIP is missing the recipe config or English UXC bank.');
  }

  const config = buildConfig(recipes);
  const english = buildUxc(baseFiles[UXC_PATH], recipes);
  const outputFiles = {};
  for (const [filename, data] of Object.entries(baseFiles)) {
    outputFiles[filename] = zipEntry(asUint8Array(data), filename);
  }
  outputFiles[CONFIG_PATH] = zipEntry(config, CONFIG_PATH);
  outputFiles[UXC_PATH] = zipEntry(english, UXC_PATH);
  return fflateLibrary.zipSync(outputFiles, { level: 6 });
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, character => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[character]);
}

function init() {
  const grid = document.getElementById('recipe-grid');
  if (!grid) return;

  let recipes = createDefaultRecipes();
  let downloadBusy = false;
  let baseFilesPromise = null;
  let collectionNotice = '';

  const enabledCount = document.getElementById('enabled-count');
  const bundleSummary = document.getElementById('bundle-summary');
  const validationSummary = document.getElementById('validation-summary');
  const downloadButton = document.getElementById('download-custom');
  const downloadButtonLabel = document.getElementById('download-button-label');
  const downloadStatus = document.getElementById('download-status');

  function numberField(recipe, key, label, min, max, help, full = false) {
    const id = `recipe-${recipe.style}-${key}`;
    return `<div class="field${full ? ' field-full' : ''}">
      <label for="${id}">${label}</label>
      <input id="${id}" data-style="${recipe.style}" data-field="${key}" type="number" inputmode="numeric" min="${min}" max="${max}" step="1" value="${recipe[key]}" required aria-describedby="${id}-hint ${id}-error">
      <span class="field-help" id="${id}-hint">${help}</span>
      <span class="field-error" id="${id}-error"></span>
    </div>`;
  }

  function selectField(recipe, key, label, options, full = false) {
    const id = `recipe-${recipe.style}-${key}`;
    return `<div class="field${full ? ' field-full' : ''}">
      <label for="${id}">${label}</label>
      <select id="${id}" data-style="${recipe.style}" data-field="${key}" aria-describedby="${id}-error">
        ${options.map(([value, text]) => `<option value="${value}"${recipe[key] === value ? ' selected' : ''}>${text}</option>`).join('')}
      </select>
      <span class="field-error" id="${id}-error"></span>
    </div>`;
  }

  function recipeCard(recipe, index) {
    const nameId = `recipe-${recipe.style}-name`;
    const [sky, horizon, sun, back, front, water] = recipe.palette;
    return `<article class="recipe-card${recipe.enabled ? '' : ' is-disabled'}" data-recipe="${recipe.style}" aria-labelledby="recipe-${recipe.style}-title">
      <div class="recipe-art" style="--art-sky:${sky};--art-horizon:${horizon};--art-sun:${sun};--art-back:${back};--art-front:${front};--art-water:${water}" aria-hidden="true">
        <span class="art-sun"></span><span class="art-hill art-hill-back"></span><span class="art-hill art-hill-front"></span><span class="art-water"></span><span class="art-grain"></span>
        <div class="art-top"><span class="art-number">${String(index + 1).padStart(2, '0')} / ${escapeHtml(recipe.slot)}</span><span class="art-state" data-role="art-state">${recipe.enabled ? 'Enabled' : 'Stock'}</span></div>
        <div class="art-bottom"><span class="art-film">${escapeHtml(recipe.film)}</span><span class="art-temp" data-role="art-temp"></span></div>
      </div>
      <div class="recipe-content">
        <div class="recipe-title-row">
          <div class="recipe-title-group"><p class="recipe-key">${recipe.key} · Creative Style ${String(recipe.style).padStart(2, '0')}</p><h3 id="recipe-${recipe.style}-title">${escapeHtml(recipe.title)}</h3></div>
          <label class="switch-label"><input type="checkbox" data-style="${recipe.style}" data-field="enabled" aria-label="Enable ${escapeHtml(recipe.title)}"${recipe.enabled ? ' checked' : ''}><span class="switch-track" data-role="enable-text">${recipe.enabled ? 'On' : 'Off'}</span></label>
        </div>
        <p class="recipe-description">${escapeHtml(recipe.description)}</p>
        <div class="recipe-tags"><span class="recipe-tag" data-role="base-tag"></span><span class="recipe-tag" data-role="wb-tag"></span></div>
        <div class="name-field field" data-name-field>
          <div class="field-label-row"><label for="${nameId}">Camera label</label><span class="byte-counter" id="${nameId}-bytes" data-role="byte-counter"></span></div>
          <div class="name-input-wrap"><input class="name-input" id="${nameId}" data-style="${recipe.style}" data-field="name" type="text" value="${escapeHtml(recipe.name)}" maxlength="13" required spellcheck="false" autocapitalize="off" autocomplete="off" aria-describedby="${nameId}-bytes ${nameId}-stock ${nameId}-error"></div>
          <span class="stock-label" id="${nameId}-stock" data-role="stock-label"></span>
          <span class="field-error" id="${nameId}-error"></span>
        </div>
        <details class="recipe-details">
          <summary>Tune this recipe</summary>
          <div class="details-body">
            <fieldset class="control-section"><legend>Starting look</legend><div class="control-grid">
              ${selectField(recipe, 'base', 'Base Creative Style', BASE_STYLES.map((text, value) => [value, `${value} · ${text}`]), true)}
            </div></fieldset>
            <fieldset class="control-section"><legend>Picture Profile</legend><div class="control-grid">
              ${numberField(recipe, 'sat', 'Saturation', -8, 7, '−8 … +7')}
              ${numberField(recipe, 'hue', 'Hue', -7, 7, '−7 … +7')}
              ${numberField(recipe, 'ppLb', 'PP LB', -9, 9, 'Amber + / blue −')}
              ${numberField(recipe, 'ppCc', 'PP CC', -9, 9, 'Magenta + / green −')}
            </div></fieldset>
            <fieldset class="control-section"><legend>White balance</legend><div class="control-grid">
              ${selectField(recipe, 'wbMode', 'WB mode', [[0, '0 · Auto'], [10, '10 · C.Temp'], [255, '0xff · Preserve']], true)}
              ${numberField(recipe, 'wbTemp', 'Color temperature', 0, 100, 'Kelvin ÷ 100 · 0 … 100', true)}
              ${numberField(recipe, 'wbLb', 'WB LB', -9, 9, 'Amber + / blue −')}
              ${numberField(recipe, 'wbCc', 'WB CC', -9, 9, 'Magenta + / green −')}
            </div></fieldset>
            <div class="details-footer"><small>Signed fields are saved as two’s-complement bytes.</small><button class="reset-recipe" type="button" data-reset-style="${recipe.style}">↺ Reset look</button></div>
          </div>
        </details>
      </div>
    </article>`;
  }

  function wbText(recipe) {
    if (recipe.wbMode === 0xff) return 'WB preserved';
    if (recipe.wbMode === 0) return 'Auto WB';
    if (recipe.wbMode === 10 && Number.isInteger(recipe.wbTemp)) return `${recipe.wbTemp * 100} K`;
    return 'C.Temp —';
  }

  function updateCard(recipe) {
    const card = grid.querySelector(`[data-recipe="${recipe.style}"]`);
    if (!card) return;
    card.classList.toggle('is-disabled', !recipe.enabled);
    card.querySelector('[data-field="enabled"]').checked = recipe.enabled;
    card.querySelector('[data-role="enable-text"]').textContent = recipe.enabled ? 'On' : 'Off';
    card.querySelector('[data-role="art-state"]').textContent = recipe.enabled ? 'Enabled' : 'Stock';
    card.querySelector('[data-role="art-temp"]').textContent = wbText(recipe);
    card.querySelector('[data-role="base-tag"]').textContent = `Base ${Number.isInteger(recipe.base) && BASE_STYLES[recipe.base] ? BASE_STYLES[recipe.base] : '—'}`;
    card.querySelector('[data-role="wb-tag"]').textContent = wbText(recipe);
    card.querySelector('[data-role="byte-counter"]').textContent = `${utf8Length(recipe.name)} / 13 bytes`;
    card.querySelector('[data-role="stock-label"]').textContent = recipe.enabled
      ? 'English Creative Style menu label'
      : `Disabled → stock menu label “${recipe.stock}”`;
  }

  function syncValidation(errors) {
    const byField = new Map(errors.filter(error => error.key !== '*').map(error => [`${error.style}.${error.key}`, error.message]));
    for (const input of grid.querySelectorAll('[data-field]')) {
      const key = `${input.dataset.style}.${input.dataset.field}`;
      const message = byField.get(key) || '';
      if (input.type !== 'checkbox') {
        input.setAttribute('aria-invalid', message ? 'true' : 'false');
        input.setCustomValidity(message);
        const container = input.closest('.field');
        if (container) container.classList.toggle('has-error', Boolean(message));
        const error = document.getElementById(`${input.id}-error`);
        if (error) error.textContent = message;
      }
    }
  }

  function updateCollection() {
    const active = recipes.filter(recipe => recipe.enabled).length;
    enabledCount.textContent = `${active} of 12 looks enabled`;
    bundleSummary.textContent = `${active} custom ${active === 1 ? 'look' : 'looks'} + neutral Standard`;

    const errors = validateRecipes(recipes);
    syncValidation(errors);
    if (errors.length) {
      validationSummary.hidden = false;
      validationSummary.classList.remove('is-success');
      validationSummary.textContent = `${errors.length} ${errors.length === 1 ? 'field needs' : 'fields need'} attention before a bundle can be built. Open the marked recipe controls to review ${errors.length === 1 ? 'it' : 'them'}.`;
    } else if (collectionNotice) {
      validationSummary.hidden = false;
      validationSummary.classList.add('is-success');
      validationSummary.textContent = collectionNotice;
    } else {
      validationSummary.hidden = true;
      validationSummary.textContent = '';
      validationSummary.classList.remove('is-success');
    }
    downloadButton.disabled = downloadBusy || errors.length > 0;
  }

  function renderAll() {
    grid.innerHTML = recipes.map(recipeCard).join('');
    for (const recipe of recipes) updateCard(recipe);
    updateCollection();
  }

  function editRecipe(input) {
    const recipe = recipes.find(item => item.style === Number(input.dataset.style));
    if (!recipe) return;
    const field = input.dataset.field;
    if (field === 'enabled') recipe.enabled = input.checked;
    else if (field === 'name') recipe.name = input.value;
    else if (input.type === 'number') recipe[field] = input.valueAsNumber;
    else recipe[field] = Number(input.value);
    collectionNotice = '';
    updateCard(recipe);
    updateCollection();
  }

  grid.addEventListener('input', event => {
    if (event.target.matches('.name-input, input[type="number"][data-field]')) editRecipe(event.target);
  });
  grid.addEventListener('change', event => {
    if (event.target.matches('select[data-field], input[type="checkbox"][data-field]')) editRecipe(event.target);
  });
  grid.addEventListener('click', event => {
    const button = event.target.closest('[data-reset-style]');
    if (!button) return;
    const style = Number(button.dataset.resetStyle);
    const index = recipes.findIndex(recipe => recipe.style === style);
    if (index < 0) return;
    const title = recipes[index].title;
    recipes[index] = { ...DEFAULT_RECIPES[index], ids: [...DEFAULT_RECIPES[index].ids], palette: [...DEFAULT_RECIPES[index].palette] };
    collectionNotice = `${title} was restored to its default values.`;
    renderAll();
    const card = grid.querySelector(`[data-recipe="${style}"]`);
    card.querySelector('details').open = true;
    card.querySelector('[data-reset-style]').focus();
  });

  document.getElementById('enable-all').addEventListener('click', () => {
    for (const recipe of recipes) { recipe.enabled = true; updateCard(recipe); }
    collectionNotice = 'All 12 film looks are enabled.';
    updateCollection();
  });
  document.getElementById('disable-all').addEventListener('click', () => {
    for (const recipe of recipes) { recipe.enabled = false; updateCard(recipe); }
    collectionNotice = 'All 12 custom looks are disabled. Their stock Creative Style names and neutral mappings will be used.';
    updateCollection();
  });
  document.getElementById('reset-all').addEventListener('click', () => {
    recipes = createDefaultRecipes();
    collectionNotice = 'The complete default collection has been restored.';
    renderAll();
  });

  async function getBaseFiles() {
    if (!baseFilesPromise) {
      baseFilesPromise = (async () => {
        if (!window.fflate || typeof window.fflate.unzipSync !== 'function') {
          throw new Error('The local ZIP tools did not load. Reload the page and try again.');
        }
        const response = await fetch(BASE_BUNDLE_URL);
        if (!response.ok) throw new Error(`The base installer could not be fetched (HTTP ${response.status}).`);
        return window.fflate.unzipSync(new Uint8Array(await response.arrayBuffer()));
      })();
    }
    try {
      return await baseFilesPromise;
    } catch (error) {
      baseFilesPromise = null;
      throw error;
    }
  }

  downloadButton.addEventListener('click', async () => {
    if (downloadBusy) return;
    downloadStatus.classList.remove('is-error');
    downloadStatus.textContent = '';
    try {
      assertValidRecipes(recipes);
    } catch (error) {
      downloadStatus.classList.add('is-error');
      downloadStatus.textContent = error.message;
      updateCollection();
      return;
    }

    downloadBusy = true;
    downloadButton.setAttribute('aria-busy', 'true');
    downloadButtonLabel.textContent = 'Building your bundle…';
    updateCollection();
    try {
      const baseFiles = await getBaseFiles();
      const zip = createCustomZip(baseFiles, recipes, window.fflate);
      const blob = new Blob([zip], { type: 'application/zip' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = 'nex7-film-recipes.zip';
      document.body.appendChild(link);
      link.click();
      link.remove();
      window.setTimeout(() => URL.revokeObjectURL(url), 1000);
      downloadStatus.textContent = `Bundle ready · ${(zip.length / 1024 / 1024).toFixed(2)} MB · your recipes are inside.`;
    } catch (error) {
      console.error('NEX-7 bundle generation failed:', error);
      downloadStatus.classList.add('is-error');
      downloadStatus.textContent = `Could not build the bundle. ${error.message}`;
    } finally {
      downloadBusy = false;
      downloadButton.removeAttribute('aria-busy');
      downloadButtonLabel.textContent = 'Download custom bundle';
      updateCollection();
    }
  });

  renderAll();
}

const Nex7Recipes = Object.freeze({
  BASE_STYLES,
  CONFIG_PATH,
  UXC_PATH,
  DEFAULT_RECIPES,
  createDefaultRecipes,
  utf8Length,
  validateRecipes,
  buildConfig,
  parseUxc,
  buildUxc,
  createCustomZip,
  init
});

if (typeof module !== 'undefined' && module.exports) module.exports = Nex7Recipes;
if (typeof window !== 'undefined') window.Nex7Recipes = Nex7Recipes;
if (typeof document !== 'undefined') {
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();
}
