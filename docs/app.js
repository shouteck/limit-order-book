// lob demo — drives the WASM build of the real C++ Book.
// embind returns int64 fields as BigInt; Number() normalizes for rendering.
const N = v => Number(v);
const $ = id => document.getElementById(id);
const toUSD = t => (N(t) / 100).toFixed(2);

let Module, book, events = [], pos = 0;
let running = false, timer = null;
let nTrades = 0, recentTrades = [];

const MAX_TRADES_SHOWN = 200;

async function init() {
    Module = await createLob();
    newTape();
    draw();
    $("play").onclick = toggle;
    $("reset").onclick = () => { stop(); newTape(); draw(); };
    $("bench").onclick = runBench;
}

function newTape() {
    if (book) book.delete();
    book = new Module.JsBook(1 << 18);
    const vec = Module.generateEvents(
        +$("count").value, +$("seed").value, 0.25, 0.05, +$("spread").value);
    events = [];
    for (let i = 0; i < vec.size(); i++) events.push(vec.get(i));
    vec.delete();
    pos = 0; nTrades = 0; recentTrades = [];
    $("ticker").innerHTML = "";
    $("benchout").textContent = "";
    updateStats();
}

function applyEvent(e) {
    const tv = book.apply(e);
    const out = [];
    for (let i = 0; i < tv.size(); i++) out.push(tv.get(i));
    tv.delete();
    return out;
}

function step(n) {
    for (let k = 0; k < n && pos < events.length; k++, pos++) {
        for (const t of applyEvent(events[pos])) {
            nTrades++;
            recentTrades.push({
                aggressor: N(events[pos].order.side) === 0 ? "buy" : "sell",
                price: N(t.price), qty: N(t.qty),
            });
        }
    }
    if (pos >= events.length) stop();
    if (recentTrades.length > MAX_TRADES_SHOWN)
        recentTrades = recentTrades.slice(-MAX_TRADES_SHOWN);
}

function toggle() { running ? stop() : play(); }
function play() {
    if (pos >= events.length) newTape();
    running = true; $("play").textContent = "Pause";
    timer = setInterval(() => {
        step(Math.max(1, Math.round(+$("speed").value / 20)));
        updateStats(); draw(); renderTicker();
    }, 50);
}
function stop() {
    running = false; $("play").textContent = "Play";
    if (timer) clearInterval(timer); timer = null;
}

function updateStats() {
    const bid = book.bestBid(), ask = book.bestAsk();
    $("s-bid").textContent = bid >= 0 ? "$" + toUSD(bid) : "-";
    $("s-ask").textContent = ask >= 0 ? "$" + toUSD(ask) : "-";
    $("s-mid").textContent = (bid >= 0 && ask >= 0)
        ? "$" + ((bid + ask) / 200).toFixed(2) : "-";
    $("s-open").textContent = book.orderCount();
    $("s-ev").textContent = pos;
    $("s-tr").textContent = nTrades;
}

function renderTicker() {
    const el = $("ticker");
    el.innerHTML = recentTrades.slice(-60).reverse().map(t =>
        `<div class="${t.aggressor}">${t.aggressor === "buy" ? "BUY " : "SELL"} ` +
        `${t.qty} @ $${toUSD(t.price)}</div>`).join("");
}

// --- depth chart --------------------------------------------------------

function draw() {
    const cv = $("chart"), ctx = cv.getContext("2d");
    const dpr = window.devicePixelRatio || 1;
    const W = cv.clientWidth, H = cv.clientHeight;
    if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);

    const bids = book.depthBid(40), asks = book.depthAsk(40);
    const B = [], A = [];
    for (let i = 0; i < bids.size(); i++) B.push(bids.get(i));
    for (let i = 0; i < asks.size(); i++) A.push(asks.get(i));
    bids.delete(); asks.delete();
    if (!B.length && !A.length) return;

    const prices = [...B, ...A].map(l => N(l.price));
    const lo = Math.min(...prices), hi = Math.max(...prices);
    const span = Math.max(1, hi - lo);
    const x = p => 20 + ((N(p) - lo) / span) * (W - 40);
    const maxQ = Math.max(...[...B, ...A].map(l => N(l.qty)), 1);
    const bh = Math.min(18, (H - 60) / Math.max(B.length, A.length, 1) - 2);

    ctx.font = "11px monospace"; ctx.textBaseline = "middle";

    const drawSide = (levels, color, fromTop) => {
        levels.forEach((l, i) => {
            const y = 30 + i * (bh + 2);
            const w = ((N(l.qty) / maxQ) * (W - 140));
            ctx.fillStyle = color + "33";                     // bar
            ctx.fillRect(20, y, w, bh);
            ctx.fillStyle = color;                            // price label
            ctx.fillText("$" + toUSD(l.price), W - 110, y + bh / 2);
            ctx.fillStyle = "#8b949e";                        // qty
            ctx.fillText(String(N(l.qty)), W - 55, y + bh / 2);
        });
    };
    drawSide(B, "#2ea043", true);
    // asks rendered below bids, in the same canvas, sorted ascending
    A.forEach((l, i) => {
        const y = 30 + (B.length + 1 + i) * (bh + 2);
        if (y > H - 20) return;
        const w = (N(l.qty) / maxQ) * (W - 140);
        ctx.fillStyle = "#f8514933";
        ctx.fillRect(20, y, w, bh);
        ctx.fillStyle = "#f85149";
        ctx.fillText("$" + toUSD(l.price), W - 110, y + bh / 2);
        ctx.fillStyle = "#8b949e";
        ctx.fillText(String(N(l.qty)), W - 55, y + bh / 2);
    });

    // divider between bids and asks
    const divY = 30 + B.length * (bh + 2) + bh / 2;
    ctx.strokeStyle = "#30363d";
    ctx.beginPath(); ctx.moveTo(20, divY); ctx.lineTo(W - 20, divY); ctx.stroke();
}

// --- benchmark ----------------------------------------------------------

async function runBench() {
    stop();
    const vec = Module.generateEvents(
        300000, +$("seed").value, 0.25, 0.05, +$("spread").value);
    const evs = [];
    for (let i = 0; i < vec.size(); i++) evs.push(vec.get(i));
    vec.delete();
    if (book) book.delete();
    book = new Module.JsBook(1 << 18);

    $("benchout").textContent = "running...";
    const t0 = performance.now();
    const CHUNK = 20000;
    for (let i = 0; i < evs.length; i += CHUNK) {
        const end = Math.min(i + CHUNK, evs.length);
        for (let j = i; j < end; j++) applyEvent(evs[j]);
        $("benchout").textContent = `running... ${end}/${evs.length}`;
        await new Promise(r => setTimeout(r, 0));   // keep UI alive
    }
    const secs = (performance.now() - t0) / 1000;
    $("benchout").textContent =
        `${(evs.length / secs / 1e6).toFixed(2)}M events/sec in-browser`;
    pos = events.length; // tape consumed; newTape() on next play
    updateStats(); draw();
}

init();
