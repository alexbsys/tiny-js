// Migrated from cmf-script sei_style_probe.js — keep in tiny-js so it survives.
// Patterns from cmf-flight-control sei-fpv scripts (no WebSocket / DOM / Worker).
// ES5-parseable so TinyJS actually runs the cases. Missing APIs are SKIP.

var passes = 0;
var fails = 0;
var skips = 0;

function report(area, name, status, detail) {
  print(status + "|" + area + "|" + name + "|" + (detail === undefined ? "" : detail) + "\n");
  if (status === "PASS") passes = passes + 1;
  else if (status === "FAIL") fails = fails + 1;
  else skips = skips + 1;
}

function tryEval(code) {
  try {
    return { ok: true, value: eval(code), err: "" };
  } catch (e) {
    return { ok: false, value: undefined, err: "" + e };
  }
}

function expectTrue(area, name, code) {
  var r = tryEval(code);
  if (!r.ok) { report(area, name, "FAIL", "throw: " + r.err); return; }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "FAIL", "got " + r.value);
}

function expectOptional(area, name, code) {
  var r = tryEval(code);
  if (!r.ok) { report(area, name, "SKIP", "throw: " + r.err); return; }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "SKIP", "absent");
}

function expectIfParsed(area, name, code) {
  var r = tryEval(code);
  if (!r.ok || r.value === undefined) {
    report(area, name, "SKIP", r.ok ? "no parse (eval swallowed)" : "no parse: " + r.err);
    return;
  }
  if (r.value) report(area, name, "PASS", "true");
  else report(area, name, "FAIL", "got " + r.value);
}

function expectFn(area, name, fn) {
  try {
    var v = fn();
    if (v) report(area, name, "PASS", "true");
    else report(area, name, "FAIL", "got " + v);
  } catch (e) {
    report(area, name, "FAIL", "throw: " + e);
  }
}

// ---------- byte buffer polyfill (what the scripts do with DataView) ----------
function ByteBuf(n) {
  this.bytes = [];
  var i;
  for (i = 0; i < n; i++) this.bytes[i] = 0;
  this.byteLength = n;
}
ByteBuf.prototype.setUint8 = function(off, v) { this.bytes[off] = v & 0xFF; };
ByteBuf.prototype.getUint8 = function(off) { return this.bytes[off] & 0xFF; };
ByteBuf.prototype.setInt8 = function(off, v) { this.bytes[off] = v & 0xFF; };
ByteBuf.prototype.getInt8 = function(off) {
  var u = this.bytes[off] & 0xFF;
  return u > 127 ? u - 256 : u;
};
ByteBuf.prototype.setUint16LE = function(off, v) {
  this.bytes[off] = v & 0xFF;
  this.bytes[off + 1] = (v >> 8) & 0xFF;
};
ByteBuf.prototype.getUint16LE = function(off) {
  return (this.bytes[off] | (this.bytes[off + 1] << 8)) & 0xFFFF;
};
ByteBuf.prototype.setUint32LE = function(off, v) {
  this.bytes[off] = v & 0xFF;
  this.bytes[off + 1] = (v >> 8) & 0xFF;
  this.bytes[off + 2] = (v >> 16) & 0xFF;
  this.bytes[off + 3] = (v >> 24) & 0xFF;
};
ByteBuf.prototype.getUint32LE = function(off) {
  return ((this.bytes[off] | (this.bytes[off + 1] << 8) |
    (this.bytes[off + 2] << 16) | (this.bytes[off + 3] << 24)) >>> 0);
};

function buildProtocolMessage(from, to, type, payload) {
  var HDR = 16;
  var buf = new ByteBuf(HDR + payload.length);
  buf.setUint8(0, 0x72);
  buf.setUint16LE(1, 0x0101);
  buf.setUint32LE(3, payload.length);
  buf.setUint32LE(7, from);
  buf.setUint32LE(11, to);
  buf.setUint8(15, type);
  var i;
  for (i = 0; i < payload.length; i++)
    buf.setInt8(i + HDR, payload.charCodeAt(i));
  return buf;
}

function parseProtocolMessage(buf) {
  var HDR = 16;
  if (buf.byteLength < HDR) return { result: false };
  if (buf.getUint8(0) !== 0x72) return { result: false };
  var ver = buf.getUint16LE(1);
  if (((ver >> 8) & 0xFF) !== 0x01) return { result: false };
  var msgLength = buf.getUint32LE(3);
  if (HDR + msgLength > buf.byteLength) return { result: false };
  var from = buf.getUint32LE(7);
  var to = buf.getUint32LE(11);
  var type = buf.getUint8(15);
  var payload = "";
  var i;
  for (i = 0; i < msgLength; i++)
    payload += String.fromCharCode(buf.getInt8(i + HDR) & 0xFF);
  return { result: true, protoVersion: ver, from: from, to: to, type: type, payload: payload };
}

// SignalingServer-style: methods hung on prototype from inside the constructor
function SignalingCore() {
  this.PROTOCOL_TYPES = { None: 0, SearchRequest: 1, ClientHello: 6, ServerHello: 7, Message: 8, Error: 127 };
  this._peerId = 0;
  this._isHelloPassed = false;
  this._knownByName = {};
  this._knownById = {};
  this._deferred = [];
  this.deferredMessageTimeoutMs = 2000;

  SignalingCore.prototype._saveKnownPeer = function(peerId, peerName) {
    this._knownByName[peerName] = { peerId: peerId, lastUpdate: Date.now() };
    this._knownById[peerId] = { peerName: peerName, lastUpdate: Date.now() };
  };
  SignalingCore.prototype._removeKnownPeerById = function(peerId) {
    var rec = this._knownById[peerId];
    if (!rec) return;
    delete this._knownById[peerId];
    delete this._knownByName[rec.peerName];
  };
  SignalingCore.prototype.isConnected = function() { return this._isHelloPassed; };
  SignalingCore.prototype.getPeerId = function() { return this._peerId; };
  SignalingCore.prototype._applyServerHello = function(buf) {
    var d = parseProtocolMessage(buf);
    if (!d.result || d.type !== this.PROTOCOL_TYPES.ServerHello) return false;
    this._peerId = d.to;
    this._isHelloPassed = true;
    return true;
  };
  SignalingCore.prototype.queueDeferred = function(peerName, payload) {
    this._deferred.push({ created: Date.now(), peerName: peerName, payload: payload });
  };
  SignalingCore.prototype.flushDeferred = function() {
    var sent = [];
    var i = 0;
    while (i < this._deferred.length) {
      var item = this._deferred[i];
      if (this._knownByName[item.peerName]) {
        this._deferred.splice(i, 1);
        sent.push(item.payload);
      } else {
        i++;
      }
    }
    return sent;
  };
}

// VideoFrame-style: prototype object literal, SMPTE math, no DOM
var FrameRates = { film: 24, PAL: 25, web: 30, high: 60 };

function VideoClock(options) {
  this.obj = options || {};
  this.frameRate = this.obj.frameRate || 24;
  this.currentTime = this.obj.currentTime || 0;
}
VideoClock.prototype = {
  get: function() {
    return Math.floor(this.currentTime * this.frameRate);
  },
  fps: FrameRates
};
VideoClock.prototype.toSeconds = function(SMPTE) {
  var time = SMPTE.split(':');
  return (((Number(time[0]) * 60) * 60) + (Number(time[1]) * 60) + Number(time[2]));
};
VideoClock.prototype.toFrames = function(SMPTE) {
  var time = SMPTE.split(':');
  var fps = this.frameRate;
  var hh = (((Number(time[0]) * 60) * 60) * fps);
  var mm = ((Number(time[1]) * 60) * fps);
  var ss = (Number(time[2]) * fps);
  var ff = Number(time[3]);
  return Math.floor(hh + mm + ss + ff);
};
VideoClock.prototype.toSMPTE = function(frame) {
  function wrap(n) { return (n < 10) ? '0' + n : '' + n; }
  var fps = this.frameRate;
  var _hour = (fps * 60) * 60;
  var _minute = fps * 60;
  var hours = Math.floor(frame / _hour);
  var minutes = Math.floor(frame / _minute) % 60;
  var seconds = Math.floor(frame / fps) % 60;
  var ff = Math.floor(frame % fps);
  return wrap(hours) + ':' + wrap(minutes) + ':' + wrap(seconds) + ':' + wrap(ff);
};
VideoClock.prototype.seekToFrame = function(frame) {
  this.currentTime = (frame / this.frameRate) + 0.00001;
  return this.get();
};

// SEI-worker-style NAL scan over a numeric array
function parseNalUnits(data) {
  var nalUnits = [];
  var i = 0;
  var len = data.length;
  while (i < len - 3) {
    var startCodeLen = 0;
    if (data[i] === 0 && data[i + 1] === 0 && data[i + 2] === 0 && data[i + 3] === 1)
      startCodeLen = 4;
    else if (data[i] === 0 && data[i + 1] === 0 && data[i + 2] === 1)
      startCodeLen = 3;
    if (startCodeLen > 0) {
      var nalStart = i + startCodeLen;
      var nalType = data[nalStart] & 0x1F;
      var nalEnd = len;
      var j;
      for (j = nalStart + 1; j < len - 2; j++) {
        if ((data[j] === 0 && data[j + 1] === 0 && data[j + 2] === 0 && data[j + 3] === 1) ||
            (data[j] === 0 && data[j + 1] === 0 && data[j + 2] === 1)) {
          nalEnd = j;
          break;
        }
      }
      var slice = [];
      var k;
      for (k = nalStart; k < nalEnd; k++) slice.push(data[k]);
      nalUnits.push({ type: nalType, offset: nalStart, size: nalEnd - nalStart, data: slice });
      i = nalEnd;
    } else {
      i++;
    }
  }
  return nalUnits;
}

function packU32LE(v) {
  return [v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF];
}

function readU32LE(arr, off) {
  return ((arr[off] | (arr[off + 1] << 8) | (arr[off + 2] << 16) | (arr[off + 3] << 24)) >>> 0);
}

function packU64LE(lo, hi) {
  return [
    lo & 0xFF, (lo >> 8) & 0xFF, (lo >> 16) & 0xFF, (lo >> 24) & 0xFF,
    hi & 0xFF, (hi >> 8) & 0xFF, (hi >> 16) & 0xFF, (hi >> 24) & 0xFF
  ];
}

// ---------- protocol / prototypes (wssignaling.js shape) ----------
expectFn("proto", "ctor_fields", function(){ var s=new SignalingCore(); return s.PROTOCOL_TYPES.Message===8 && s.isConnected()===false && s.getPeerId()===0; });
expectFn("proto", "methods_on_instance", function(){ var s=new SignalingCore(); return typeof s._saveKnownPeer==='function' && typeof s.flushDeferred==='function'; });
expectFn("proto", "peer_cache", function(){ var s=new SignalingCore(); s._saveKnownPeer(1234,'cam1'); return s._knownById[1234].peerName==='cam1' && s._knownByName.cam1.peerId===1234; });
expectFn("proto", "peer_remove", function(){ var s=new SignalingCore(); s._saveKnownPeer(7,'x'); s._removeKnownPeerById(7); return s._knownById[7]===undefined && s._knownByName.x===undefined; });
expectFn("proto", "deferred_flush", function(){ var s=new SignalingCore(); s.queueDeferred('cam', 'hi'); s.queueDeferred('other','no'); s._saveKnownPeer(1,'cam'); var sent=s.flushDeferred(); return sent.length===1 && sent[0]==='hi' && s._deferred.length===1; });
expectFn("proto", "two_instances", function(){ var a=new SignalingCore(); var b=new SignalingCore(); a._saveKnownPeer(1,'a'); b._saveKnownPeer(2,'b'); return a._knownById[1].peerName==='a' && b._knownById[2].peerName==='b' && a._knownById[2]===undefined; });

expectFn("wire", "roundtrip_hello", function(){ var s=new SignalingCore(); var buf=buildProtocolMessage(0, 99, s.PROTOCOL_TYPES.ServerHello, 'peerweb1'); return s._applyServerHello(buf) && s._peerId===99 && s.isConnected()===true; });
expectFn("wire", "roundtrip_payload", function(){ var buf=buildProtocolMessage(10, 20, 8, 'Hello world!'); var d=parseProtocolMessage(buf); return d.result && d.from===10 && d.to===20 && d.type===8 && d.payload==='Hello world!'; });
expectFn("wire", "reject_short", function(){ var buf=new ByteBuf(8); return parseProtocolMessage(buf).result===false; });
expectFn("wire", "reject_bad_magic", function(){ var buf=buildProtocolMessage(1,2,8,'x'); buf.setUint8(0, 0x00); return parseProtocolMessage(buf).result===false; });
expectFn("wire", "le_uint32", function(){ var b=new ByteBuf(4); b.setUint32LE(0, 0x01020304); return b.getUint8(0)===4 && b.getUint8(3)===1 && b.getUint32LE(0)===0x01020304; });
expectFn("wire", "charCode_roundtrip", function(){ var s='CMF_'; var out=''; var i; for(i=0;i<s.length;i++) out+=String.fromCharCode(s.charCodeAt(i)); return out==='CMF_'; });

// ---------- VideoClock (VideoFrame.js math, no DOM) ----------
expectFn("video", "default_fps", function(){ var v=new VideoClock(); return v.frameRate===24 && v.fps.web===30; });
expectFn("video", "options_or", function(){ var v=new VideoClock({frameRate:25, currentTime:2}); return v.frameRate===25 && v.get()===50; });
expectFn("video", "smpte_roundtrip", function(){ var v=new VideoClock({frameRate:25}); var s=v.toSMPTE(75); return s==='00:00:03:00' && v.toFrames(s)===75; });
expectFn("video", "toSeconds", function(){ var v=new VideoClock({frameRate:25}); return v.toSeconds('00:01:12:00')===72; });
expectFn("video", "seek_frame", function(){ var v=new VideoClock({frameRate:25}); v.seekToFrame(50); return v.get()===50; });
expectFn("video", "replace_callback", function(){ var s='hh:mm'.replace(/hh|mm/g, function(tok){ return tok==='hh'?'01':'02'; }); return s==='01:02'; });
expectFn("video", "object_keys_option", function(){ var obj={frame:1750}; var k=Object.keys(obj)[0]; return k==='frame'; });
expectFn("video", "number_split", function(){ var n=(75/25); var whole=Number(String(n).split('.')[0]); return whole===3; });

// ---------- NAL / SEI scan (sei-worker.js shape, numeric arrays) ----------
expectFn("nal", "start_code4", function(){ var d=[0,0,0,1, 0x65, 9,9, 0,0,0,1, 0x06, 5]; var n=parseNalUnits(d); return n.length===2 && n[0].type===5 && n[1].type===6 && n[0].size===3; });
expectFn("nal", "start_code3", function(){ var d=[0,0,1, 0x41, 1, 0,0,1, 0x01, 2]; var n=parseNalUnits(d); return n.length===2 && n[0].type===1 && n[1].type===1; });
expectFn("nal", "uuid_bytes", function(){ var uuid=[0x43,0x4D,0x46,0x5F,0x43,0x41,0x50,0x54,0x55,0x52,0x45,0x5F,0x54,0x53,0x5F,0x5F]; var s=''; var i; for(i=0;i<4;i++) s+=String.fromCharCode(uuid[i]); return s==='CMF_' && uuid.length===16; });
expectFn("nal", "readU32_le", function(){ var a=packU32LE(0x12345678); return readU32LE(a,0)===0x12345678; });
expectFn("nal", "u64_lo_hi", function(){ var t=packU64LE(0x89ABCDEF, 0x01234567); return t[0]===0xEF && t[7]===0x01 && t.length===8; });
expectFn("nal", "and_mask_naltype", function(){ return (0x65 & 0x1F)===5 && (0x06 & 0x1F)===6; });
expectFn("nal", "ushift_u32", function(){ var v = (0xFF | (0xFF<<8) | (0xFF<<16) | (0xFF<<24)) >>> 0; return v===4294967295; });

// ---------- host-like APIs used by those files ----------
expectOptional("host", "ArrayBuffer", "typeof ArrayBuffer==='function'");
expectOptional("host", "DataView", "typeof DataView==='function'");
expectOptional("host", "Uint8Array", "typeof Uint8Array==='function'");
expectOptional("host", "Map", "typeof Map==='function'");
expectOptional("host", "BigInt_ctor", "typeof BigInt==='function'");
expectOptional("host", "toFixed", "typeof (1.5).toFixed==='function' && (1.5).toFixed(1)==='1.5'");
expectOptional("host", "setTimeout", "typeof setTimeout==='function'");
expectOptional("host", "performance_now", "typeof performance==='object' && typeof performance.now==='function'");
expectOptional("host", "Object_keys", "typeof Object.keys==='function' && Object.keys({a:1,b:2}).length===2");

expectIfParsed("syntax", "arrow_lexical", "(function(){ var o={n:3,f:null}; o.f=function(){ return (()=>this.n)(); }; return o.f()===3; })()");
expectIfParsed("syntax", "for_of_pairs", "(function(){ var s=0; for (const x of [1,2,3]) s+=x; return s===6; })()");
expectIfParsed("syntax", "destructure_rename", "(function(){ var rec={value:9,done:false}; var {value: frame, done: d}=rec; return frame===9 && d===false; })()");
expectIfParsed("syntax", "template_log", "(function(){ var n=2; return `size: ${n}`==='size: 2'; })()");
expectIfParsed("syntax", "nullish", "(function(){ var a=null; var b=0; return (a??7)===7 && (b??7)===0; })()");
expectIfParsed("syntax", "optchain", "(function(){ var o={a:{b:1}}; return o.a?.b===1 && o.z?.b===undefined; })()");

print("SUMMARY|pass="+passes+" fail="+fails+" skip="+skips+"\n");
print("FAILS="+fails+"\n");

result = (fails === 0);

