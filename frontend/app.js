'use strict';
const $ = id => document.getElementById(id);
const scale = 1000000000n;
let paused = false, latest = null, shown = null, lastChange = 0, lastKey = '', rows = [], rateSamples = [], currentRun = '';
function price(raw) {
  const value = BigInt(raw), absolute = value < 0n ? -value : value;
  const fraction = (absolute % scale).toString().padStart(9, '0').replace(/0+$/, '').padEnd(2, '0');
  return `${value < 0n ? '-' : ''}${absolute / scale}.${fraction}`;
}
const quantity = raw => BigInt(raw).toLocaleString('en-US');
function status(text, kind = '') {
  $('status').textContent = text;
  $('status-dot').className = `dot ${kind}`;
}
function makeRow() {
  const row = document.createElement('tr');
  for (let i = 0; i < 8; ++i) {
    const cell = document.createElement('td');
    if (i === 3 || i === 4) {
      const bar = document.createElement('div');
      bar.className = `bar ${i === 3 ? 'bid-bar' : 'ask-bar'}`;
      cell.append(bar);
    }
    cell.append(document.createElement('span'));
    row.append(cell);
  }
  $('depth').append(row);
  return row;
}
function render(data) {
  shown = data;
  $('mode').textContent = data.mode === 'simulation' ? 'SIMULATION' : 'REPLAY';
  $('mode').className = data.mode === 'simulation' ? 'mode simulation' : 'mode';
  $('source').textContent = data.mode === 'simulation' ? 'Synthetic feed → C++ book worker → depth snapshots' : 'Recorded events → reconstructed depth';
  if (currentRun !== data.run_id) { currentRun = data.run_id; rateSamples = []; }
  const now = Date.now();
  if (!rateSamples.length || rateSamples.at(-1).processed !== data.processed) {
    rateSamples.push({time:now,processed:data.processed});
    rateSamples = rateSamples.filter(sample => now - sample.time < 5000);
  }
  if (rateSamples.length > 1 && rateSamples.at(-1).time - rateSamples[0].time >= 1000) {
    const first = rateSamples[0], last = rateSamples.at(-1);
    $('rate').textContent = data.complete ? '0.0' : ((last.processed-first.processed)/((last.time-first.time)/1000)).toFixed(1);
  } else $('rate').textContent = data.complete ? '0.0' : '—';
  const bids = data.bids.slice(0, Number($('levels').value));
  const asks = data.asks.slice(0, Number($('levels').value));
  $('best-bid').textContent = data.bids.length ? price(data.bids[0].price) : '—';
  $('best-ask').textContent = data.asks.length ? price(data.asks[0].price) : '—';
  $('spread').textContent = data.bids.length && data.asks.length
    ? price(BigInt(data.asks[0].price) - BigInt(data.bids[0].price)) : '—';
  $('active').textContent = data.active_orders.toLocaleString('en-US');
  $('processed').textContent = data.processed.toLocaleString('en-US');
  const count = Math.max(bids.length, asks.length);
  $('empty').hidden = count > 0;
  while (rows.length < count) rows.push(makeRow());
  while (rows.length > count) rows.pop().remove();
  let bidTotal = 0n, askTotal = 0n;
  const maxQty = Math.max(1, ...bids.concat(asks).map(level => Number(level.quantity)));
  for (let i = 0; i < count; ++i) {
    const bid = bids[i], ask = asks[i];
    if (bid) bidTotal += BigInt(bid.quantity);
    if (ask) askTotal += BigInt(ask.quantity);
    const values = [bid ? quantity(bidTotal) : '—', bid?.orders ?? '—', bid ? quantity(bid.quantity) : '—', bid ? price(bid.price) : '—', ask ? price(ask.price) : '—', ask ? quantity(ask.quantity) : '—', ask?.orders ?? '—', ask ? quantity(askTotal) : '—'];
    [...rows[i].children].forEach((cell, column) => {
      const span = cell.querySelector('span'), next = String(values[column]);
      if (span.textContent !== next) {
        span.textContent = next;
        cell.classList.remove('changed');
        void cell.offsetWidth;
        cell.classList.add('changed');
      }
    });
    rows[i].children[3].querySelector('.bar').style.width = `${bid ? Number(bid.quantity) / maxQty * 400 : 0}%`;
    rows[i].children[4].querySelector('.bar').style.width = `${ask ? Number(ask.quantity) / maxQty * 400 : 0}%`;
  }
  renderDepthChart(data);
  renderTape(data);
  const event = data.last_event;
  $('event').textContent = data.processed
    ? `${event.action} · Order ${event.order_id} · ${quantity(event.size)} shares` : '—';
  if (!count && data.complete) {
    $('empty').querySelector('h3').textContent = 'Replay finished with an empty book';
    $('empty').querySelector('p').textContent = 'There are no active price levels to display.';
  } else {
    $('empty').querySelector('h3').textContent = 'Waiting for the first snapshot';
    $('empty').querySelector('p').textContent = 'Start a replay with snapshot output to see the book here.';
  }
  $('empty').querySelector('code').hidden = !!data.complete;
}
function refreshStatus() {
  if (paused) return status('View paused');
  if (!shown) return;
  if (Date.now() - lastChange > 3000) $('rate').textContent = '0.0';
  status(shown.complete ? 'Replay complete' : Date.now() - lastChange > 3000 ? 'No new events' : shown.mode === 'simulation' ? 'Live simulation' : 'Replaying', shown.complete ? 'complete' : 'live');
  $('updated').textContent = `Snapshot updated ${Math.max(0, Math.floor((Date.now() - lastChange) / 1000))}s ago`;
}
async function poll() {
  try {
    const response = await fetch(`state.json?t=${Date.now()}`, {cache: 'no-store'});
    if (!response.ok) {
      status(response.status === 404 ? 'Waiting for replay' : 'Snapshot unavailable', response.status === 404 ? '' : 'error');
      return;
    }
    const data = await response.json();
    if (!Array.isArray(data.bids) || !Array.isArray(data.asks)) throw new Error('Invalid depth');
    latest = data;
    const key = `${data.run_id}:${data.processed}:${data.complete}`;
    if (key !== lastKey) {
      lastKey = key;
      lastChange = Date.now();
      if (!paused) render(data);
    }
    refreshStatus();
  } catch {
    status('Connection interrupted', 'error');
  } finally {
    setTimeout(poll, 250);
  }
}
$('pause').addEventListener('click', () => {
  paused = !paused;
  $('pause').textContent = paused ? 'Resume view' : 'Pause view';
  $('pause').setAttribute('aria-pressed', String(paused));
  if (!paused && latest) { rateSamples = []; render(latest); }
  refreshStatus();
});
$('levels').addEventListener('change', () => { if (shown) render(shown); });
poll();

function renderDepthChart(data) {
  const svg = $('chart');
  const bids = data.bids.slice(0, Number($('levels').value));
  const asks = data.asks.slice(0, Number($('levels').value));
  const sum = levels => levels.reduce((n, l) => n + BigInt(l.quantity), 0n);
  const bidTotal = sum(bids), askTotal = sum(asks), total = bidTotal + askTotal;
  $('liquidity').textContent = total ? `${Number(bidTotal * 100n / total)}% bid liquidity · visible levels` : '—';
  svg.replaceChildren();
  if (!bids.length && !asks.length) return;
  const prices = bids.concat(asks).map(l => BigInt(l.price));
  const min = prices.reduce((a,b) => a < b ? a : b), max = prices.reduce((a,b) => a > b ? a : b);
  const range = max - min || 10000000n;
  const ceiling = Math.max(1, Number(bidTotal), Number(askTotal));
  const x = p => 12 + Number(BigInt(p) - min) / Number(range) * 736;
  const y = q => 116 - Number(q) / ceiling * 100;
  const element = (name, attributes) => {
    const node = document.createElementNS('http://www.w3.org/2000/svg', name);
    for (const [key,value] of Object.entries(attributes)) node.setAttribute(key,value);
    svg.append(node);
    return node;
  };
  element('line',{x1:12,y1:116,x2:748,y2:116,stroke:'#36464c'});
  const curve = (levels, color, reverse) => {
    let running = 0n;
    let points = levels.map(l => { running += BigInt(l.quantity); return [x(l.price),y(running)]; });
    if (reverse) points.reverse();
    if (!points.length) return;
    const path = points.map((p,i) => i ? `H${p[0]}V${p[1]}` : `M${p[0]},${p[1]}`).join(' ');
    element('path',{d:`${path} L${points.at(-1)[0]},116 L${points[0][0]},116 Z`,fill:color,'fill-opacity':'.12'});
    element('path',{d:path,fill:'none',stroke:color,'stroke-width':2});
  };
  curve(bids,'#73dbab',true); curve(asks,'#f2929d',false);
  element('text',{x:12,y:139,'text-anchor':'start'}).textContent = price(min);
  element('text',{x:748,y:139,'text-anchor':'end'}).textContent = price(max);
  element('text',{x:380,y:139,'text-anchor':'middle'}).textContent = 'Price';
}

function renderTape(data) {
  const list = $('tape');
  const events = (data.recent_events || []).slice(-12).reverse();
  list.replaceChildren();
  for (const [index,event] of events.entries()) {
    const item = document.createElement('li');
    if (index < 3) item.className = 'event-new';
    const left = document.createElement('div'), right = document.createElement('div');
    right.className = 'right';
    const action = document.createElement('span');
    action.className = `event-action event-${event.action.toLowerCase()}`;
    action.textContent = event.action.toUpperCase();
    const id = document.createElement('small'); id.textContent = `${event.side} · #${event.order_id}`;
    left.append(action,id);
    const value = document.createElement('span'); value.textContent = price(event.price);
    value.className = event.side === 'Bid' ? 'bid' : 'ask';
    const size = document.createElement('small'); size.textContent = `${quantity(event.size)} shares`;
    right.append(value,size); item.append(left,right); list.append(item);
  }
  if (!events.length) {
    const item = document.createElement('li'); item.textContent = 'Waiting for events'; list.append(item);
  }
}
