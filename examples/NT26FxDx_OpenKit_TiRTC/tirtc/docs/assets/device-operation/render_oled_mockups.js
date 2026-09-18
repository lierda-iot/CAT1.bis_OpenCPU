/*
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Deterministically renders the documentation OLED mockups from the same
 * 5x7 glyph data, spacing and coordinates used by src/ui/*.c.
 */

"use strict";

const fs = require("node:fs");
const path = require("node:path");

const OUT_DIR = __dirname;
const SCREEN_WIDTH = 128;
const SCREEN_HEIGHT = 64;

const digitRows = [
  [0x3e, 0x51, 0x49, 0x45, 0x3e],
  [0x00, 0x42, 0x7f, 0x40, 0x00],
  [0x42, 0x61, 0x51, 0x49, 0x46],
  [0x21, 0x41, 0x45, 0x4b, 0x31],
  [0x18, 0x14, 0x12, 0x7f, 0x10],
  [0x27, 0x45, 0x45, 0x45, 0x39],
  [0x3c, 0x4a, 0x49, 0x49, 0x30],
  [0x01, 0x71, 0x09, 0x05, 0x03],
  [0x36, 0x49, 0x49, 0x49, 0x36],
  [0x06, 0x49, 0x49, 0x29, 0x1e],
];

const upperRows = [
  [0x7e, 0x11, 0x11, 0x11, 0x7e],
  [0x7f, 0x49, 0x49, 0x49, 0x36],
  [0x3e, 0x41, 0x41, 0x41, 0x22],
  [0x7f, 0x41, 0x41, 0x22, 0x1c],
  [0x7f, 0x49, 0x49, 0x49, 0x41],
  [0x7f, 0x09, 0x09, 0x09, 0x01],
  [0x3e, 0x41, 0x49, 0x49, 0x7a],
  [0x7f, 0x08, 0x08, 0x08, 0x7f],
  [0x00, 0x41, 0x7f, 0x41, 0x00],
  [0x20, 0x40, 0x41, 0x3f, 0x01],
  [0x7f, 0x08, 0x14, 0x22, 0x41],
  [0x7f, 0x40, 0x40, 0x40, 0x40],
  [0x7f, 0x02, 0x0c, 0x02, 0x7f],
  [0x7f, 0x04, 0x08, 0x10, 0x7f],
  [0x3e, 0x41, 0x41, 0x41, 0x3e],
  [0x7f, 0x09, 0x09, 0x09, 0x06],
  [0x3e, 0x41, 0x51, 0x21, 0x5e],
  [0x7f, 0x09, 0x19, 0x29, 0x46],
  [0x46, 0x49, 0x49, 0x49, 0x31],
  [0x01, 0x01, 0x7f, 0x01, 0x01],
  [0x3f, 0x40, 0x40, 0x40, 0x3f],
  [0x1f, 0x20, 0x40, 0x20, 0x1f],
  [0x7f, 0x20, 0x18, 0x20, 0x7f],
  [0x63, 0x14, 0x08, 0x14, 0x63],
  [0x03, 0x04, 0x78, 0x04, 0x03],
  [0x61, 0x51, 0x49, 0x45, 0x43],
];

const blank = [0, 0, 0, 0, 0];
const punctuation = new Map([
  [":", [0x00, 0x36, 0x36, 0x00, 0x00]],
  ["-", [0x08, 0x08, 0x08, 0x08, 0x08]],
  ["+", [0x08, 0x08, 0x3e, 0x08, 0x08]],
  [">", [0x00, 0x41, 0x22, 0x14, 0x08]],
]);

function glyphFor(ch) {
  if (ch >= "0" && ch <= "9") {
    return digitRows[ch.charCodeAt(0) - "0".charCodeAt(0)];
  }
  if (ch >= "A" && ch <= "Z") {
    return upperRows[ch.charCodeAt(0) - "A".charCodeAt(0)];
  }
  return punctuation.get(ch) || blank;
}

function createCanvas() {
  return Array.from({ length: SCREEN_HEIGHT }, () =>
    Array(SCREEN_WIDTH).fill(false),
  );
}

function drawText(canvas, x, y, text, scale = 1) {
  for (const ch of text) {
    const glyph = glyphFor(ch);
    for (let col = 0; col < 5; col += 1) {
      for (let row = 0; row < 7; row += 1) {
        if ((glyph[col] & (1 << row)) === 0) continue;
        for (let dx = 0; dx < scale; dx += 1) {
          for (let dy = 0; dy < scale; dy += 1) {
            const px = x + col * scale + dx;
            const py = y + row * scale + dy;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
              canvas[py][px] = true;
            }
          }
        }
      }
    }
    x += 6 * scale;
  }
}

function drawCentered(canvas, y, text, scale = 1) {
  const width = text.length * 6 * scale;
  const x = width < SCREEN_WIDTH ? Math.floor((SCREEN_WIDTH - width) / 2) : 0;
  drawText(canvas, x, y, text, scale);
}

function startup(title, mainText, footer = "") {
  const canvas = createCanvas();
  drawCentered(canvas, 0, title, 1);
  drawCentered(canvas, 20, mainText, 2);
  if (footer) drawCentered(canvas, 50, footer, 1);
  return canvas;
}

function home() {
  const canvas = createCanvas();
  drawText(canvas, 0, 0, "4G:OK", 1);
  drawText(canvas, 42, 0, "WX:OK", 1);
  drawText(canvas, 96, 0, "14:35", 1);
  drawText(canvas, 0, 12, ">AI", 2);
  drawText(canvas, 68, 12, " WX", 2);
  drawText(canvas, 0, 40, " DEV", 2);
  drawText(canvas, 68, 40, " SET", 2);
  return canvas;
}

function settings() {
  const canvas = createCanvas();
  drawCentered(canvas, 0, "SETTINGS", 1);
  drawText(canvas, 0, 12, ">V+", 2);
  drawText(canvas, 48, 12, " V-", 2);
  drawText(canvas, 0, 40, " M+", 2);
  drawText(canvas, 48, 40, " M-", 2);
  drawText(canvas, 104, 12, "8", 2);
  drawText(canvas, 104, 40, "10", 2);
  return canvas;
}

function ai(status) {
  const canvas = createCanvas();
  drawCentered(canvas, 8, "AI CHAT", 2);
  drawCentered(canvas, 36, status, 1);
  drawCentered(canvas, 50, "KEY2 BACK", 1);
  return canvas;
}

function wxContact() {
  const canvas = createCanvas();
  drawCentered(canvas, 3, "WX 1/2", 1);
  drawCentered(canvas, 21, "ALICE", 2);
  drawCentered(canvas, 53, "K0 NEXT K1 CALL", 1);
  return canvas;
}

function wxState(status, footer = "K2 HANGUP", name = "ALICE") {
  const canvas = createCanvas();
  drawCentered(canvas, 2, "WECHAT", 1);
  drawCentered(canvas, 18, name, 2);
  drawCentered(canvas, 43, status, 1);
  drawCentered(canvas, 54, footer, 1);
  return canvas;
}

function wxNoContact() {
  const canvas = createCanvas();
  drawCentered(canvas, 3, "WECHAT", 1);
  drawCentered(canvas, 23, "NO CONTACT", 2);
  drawCentered(canvas, 53, "K2 BACK", 1);
  return canvas;
}

function devContact() {
  const canvas = createCanvas();
  drawCentered(canvas, 3, "DEV 1/2", 1);
  drawCentered(canvas, 21, "ROOM2", 2);
  drawCentered(canvas, 43, "ONLINE", 1);
  drawCentered(canvas, 53, "K0 NEXT K1 CALL", 1);
  return canvas;
}

function devState(status, footer = "K2 HANGUP", name = "ROOM2") {
  const canvas = createCanvas();
  drawCentered(canvas, 2, "DEVICE CALL", 1);
  drawCentered(canvas, 18, name, 2);
  drawCentered(canvas, 43, status, 1);
  drawCentered(canvas, 54, footer, 1);
  return canvas;
}

function devNoDevice() {
  const canvas = createCanvas();
  drawCentered(canvas, 3, "DEVICE", 1);
  drawCentered(canvas, 23, "NO DEVICE", 2);
  drawCentered(canvas, 53, "K2 BACK", 1);
  return canvas;
}

function uiReady() {
  const canvas = createCanvas();
  drawCentered(canvas, 26, "UI READY", 1);
  return canvas;
}

function toSvg(canvas, title) {
  let pixelPath = "";
  for (let y = 0; y < SCREEN_HEIGHT; y += 1) {
    for (let x = 0; x < SCREEN_WIDTH; x += 1) {
      if (canvas[y][x]) pixelPath += `M${x} ${y}h1v1h-1z`;
    }
  }

  return `<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="264" viewBox="-1 -1 130 66" role="img" aria-label="${title}">
  <title>${title}</title>
  <rect x="-1" y="-1" width="130" height="66" rx="2" fill="#101820"/>
  <rect width="128" height="64" fill="#dffbff"/>
  <path d="${pixelPath}" fill="#07151c" shape-rendering="crispEdges"/>
</svg>
`;
}

const screens = [
  ["oled-00-all-pixels.svg", "OLED 全亮自检", createCanvas()],
  ["oled-01-ui-ready.svg", "UI READY", uiReady()],
  ["oled-02-wait-lte.svg", "NETWORK WAIT LTE", startup("NETWORK", "WAIT LTE", "4G REQUIRED")],
  ["oled-03-start-pdp.svg", "NETWORK START PDP", startup("NETWORK", "START PDP", "4G REQUIRED")],
  ["oled-04-service.svg", "BINDING SERVICE GET SERVER", startup("BINDING", "SERVICE", "GET SERVER")],
  ["oled-05-get-code.svg", "BINDING GET CODE", startup("BINDING", "GET CODE")],
  ["oled-06-bind-code.svg", "BINDING 验证码", startup("BINDING", "123456", "BIND WEB 187")],
  ["oled-07-check-server.svg", "BINDING CHECK SERVER", startup("BINDING", "CHECK", "SERVER")],
  ["oled-08-home.svg", "主页", home()],
  ["oled-09-ai-listening.svg", "AI LISTENING", ai("LISTENING")],
  ["oled-10-ai-speak.svg", "AI SPEAK", ai("AI SPEAK")],
  ["oled-11-wx-contact.svg", "WX 联系人", wxContact()],
  ["oled-12-wx-incoming.svg", "WX 来电", wxState("INCOMING", "K1 ANS K2 END")],
  ["oled-13-wx-talk.svg", "WX 通话", wxState("WX TALK")],
  ["oled-14-dev-contact.svg", "DEV 联系人", devContact()],
  ["oled-15-dev-incoming.svg", "DEV 来电", devState("INCOMING", "K1 ANSWER K2 END")],
  ["oled-16-dev-wait-peer.svg", "DEV WAIT PEER", devState("WAIT PEER")],
  ["oled-17-dev-talk.svg", "DEV TALK", devState("DEV TALK")],
  ["oled-18-settings.svg", "SETTINGS", settings()],
  ["oled-19-network-retry.svg", "NETWORK NET RETRY", startup("NETWORK", "NET RETRY", "4G REQUIRED")],
  ["oled-20-bind-error.svg", "BINDING BIND ERR", startup("BINDING", "BIND ERR", "ERR -109 KEY1")],
  ["oled-21-wx-no-contact.svg", "WX NO CONTACT", wxNoContact()],
  ["oled-22-dev-no-device.svg", "DEV NO DEVICE", devNoDevice()],
  ["oled-23-unbound.svg", "UNBOUND RESTART", startup("UNBOUND", "RESTART", "KEEP DEVICE ID")],
  ["oled-24-ai-error.svg", "AI 错误", ai("ERR -1001")],
  ["oled-25-wx-error.svg", "WX 错误", wxState("ERR -3007")],
  ["oled-26-dev-error.svg", "DEV 错误", devState("ERR -5009", "K2 BACK")],
];

fs.mkdirSync(OUT_DIR, { recursive: true });
for (const [filename, title, canvas] of screens) {
  fs.writeFileSync(path.join(OUT_DIR, filename), toSvg(canvas, title), "utf8");
}

console.log(`Generated ${screens.length} OLED mockups in ${OUT_DIR}`);
