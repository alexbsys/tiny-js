#include "TinyJS_Debug.h"
#include "TinyJS.h"

#include <algorithm>
#include <cctype>

static std::string NormalizePath(std::string s) {
  for (char& ch : s) {
    if (ch == '\\')
      ch = '/';
  }
  return s;
}

static std::string Basename(const std::string& s) {
  const auto pos = s.find_last_of('/');
  return pos == std::string::npos ? s : s.substr(pos + 1);
}

CTinyJSDebug::CTinyJSDebug() = default;
CTinyJSDebug::~CTinyJSDebug() {
  paused_ = false;
  cv_.notify_all();
}

void CTinyJSDebug::setEnabled(bool on) {
  enabled_ = on;
}

void CTinyJSDebug::setHook(CTinyJSDebugHook hook, void* user) {
  hook_ = hook;
  hook_user_ = user;
}

void CTinyJSDebug::addBreakpoint(const std::string& file, int line_1based) {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& bp : breakpoints_) {
    if (bp.line == line_1based && filesMatch(bp.file, file))
      return;
  }
  breakpoints_.push_back({ file, line_1based });
}

void CTinyJSDebug::removeBreakpoint(const std::string& file, int line_1based) {
  std::lock_guard<std::mutex> lock(mu_);
  breakpoints_.erase(
    std::remove_if(breakpoints_.begin(), breakpoints_.end(),
                   [&](const Breakpoint& bp) {
                     return bp.line == line_1based && filesMatch(bp.file, file);
                   }),
    breakpoints_.end());
}

void CTinyJSDebug::clearBreakpoints() {
  std::lock_guard<std::mutex> lock(mu_);
  breakpoints_.clear();
}

void CTinyJSDebug::requestPause() {
  pause_requested_.store(true);
}

void CTinyJSDebug::debugContinue() {
  resume(CTinyJSDebugStep::Continue);
}

void CTinyJSDebug::debugStepIn() {
  resume(CTinyJSDebugStep::In);
}

void CTinyJSDebug::debugStepOver() {
  resume(CTinyJSDebugStep::Over);
}

void CTinyJSDebug::debugStepOut() {
  resume(CTinyJSDebugStep::Out);
}

void CTinyJSDebug::resume(CTinyJSDebugStep next) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    step_mode_ = next;
    resume_file_ = last_loc_.file;
    resume_line_ = last_loc_.line;
    resume_depth_ = static_cast<int>(frames_.size());
    paused_ = false;
  }
  cv_.notify_all();
}

bool CTinyJSDebug::filesMatch(const std::string& a, const std::string& b) {
  if (a.empty() || b.empty())
    return a == b;
  std::string na = NormalizePath(a);
  std::string nb = NormalizePath(b);
  if (na == nb)
    return true;
  if (Basename(na) == Basename(nb) &&
      (na.find('/') == std::string::npos || nb.find('/') == std::string::npos))
    return true;
  if (na.size() > nb.size())
    std::swap(na, nb);
  if (nb.size() > na.size() &&
      nb.compare(nb.size() - na.size(), na.size(), na) == 0 &&
      nb[nb.size() - na.size() - 1] == '/')
    return true;
  return false;
}

static bool IsSyntheticDebugToken(int tk) {
  return tk == LEX_T_SKIP || tk == LEX_T_FORWARD || tk == '{' || tk == ';' ||
         tk == LEX_EOF || tk == LEX_T_DUMMY_LABEL;
}

static bool FillLocFromTokens(CTinyJSDebugLoc& loc, const TOKEN_VECT& tokens) {
  for (const auto& tok : tokens) {
    if (IsSyntheticDebugToken(tok.token))
      continue;
    if (tok.token == LEX_T_LOOP || tok.token == LEX_T_FOR_IN || tok.token == LEX_T_TRY)
      continue;
    loc.line = tok.line + 1;
    loc.column = tok.column + 1;
    return true;
  }
  return false;
}

CTinyJSDebugLoc CTinyJSDebug::locFromTokenizer(CTinyJS* js) {
  CTinyJSDebugLoc loc;
  if (!js || !js->t)
    return loc;
  loc.file = js->t->currentFile;

  CScriptToken& tok = js->t->getToken();
  loc.line = tok.line + 1;
  loc.column = tok.column + 1;

  if (tok.token == LEX_T_LOOP || tok.token == LEX_T_FOR_IN) {
    CScriptTokenDataLoop& loop = tok.Loop();
    if (!FillLocFromTokens(loc, loop.init) &&
        !FillLocFromTokens(loc, loop.condition) &&
        !FillLocFromTokens(loc, loop.body) &&
        !FillLocFromTokens(loc, loop.iter)) {
      loc.line = tok.line + 1;
      loc.column = tok.column + 1;
    }
  } else if (tok.token == LEX_T_TRY) {
    const CScriptTokenDataTry& tr = tok.Try();
    if (!FillLocFromTokens(loc, tr.tryBlock)) {
      loc.line = tok.line + 1;
      loc.column = tok.column + 1;
    }
  } else if (tok.token == LEX_T_SKIP) {
    const CScriptTokenizer::ScriptTokenPosition& pos = js->t->getPos();
    auto it = pos.pos;
    if (it != pos.tokens->end()) {
      ++it;
      while (it != pos.tokens->end() && IsSyntheticDebugToken(it->token))
        ++it;
      if (it != pos.tokens->end()) {
        loc.line = it->line + 1;
        loc.column = it->column + 1;
      }
    }
  }

  const std::string& tk = js->t->tkStr();
  if (!tk.empty())
    loc.end_column = loc.column + static_cast<int>(tk.size());
  return loc;
}

bool CTinyJSDebug::hitBreakpoint(const CTinyJSDebugLoc& loc) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& bp : breakpoints_) {
    if (bp.line == loc.line && filesMatch(bp.file, loc.file))
      return true;
  }
  return false;
}

void CTinyJSDebug::stopAndWait(CTinyJS* js, CTinyJSDebugStop reason, const CTinyJSDebugLoc& loc) {
  last_stop_ = reason;
  last_loc_ = loc;
  if (!hook_)
    return;
  paused_ = true;
  hook_(js, reason, loc, hook_user_);
  std::unique_lock<std::mutex> lock(mu_);
  while (paused_)
    cv_.wait(lock);
}

void CTinyJSDebug::onStatement(CTinyJS* js) {
  if (!js || !js->t)
    return;
  const int tk = js->t->tk;
  if (tk == '{' || tk == ';' || tk == LEX_T_FORWARD || tk == LEX_EOF ||
      tk == LEX_T_SKIP || tk == LEX_T_DUMMY_LABEL)
    return;

  const CTinyJSDebugLoc loc = locFromTokenizer(js);

  CTinyJSDebugStep mode;
  std::string resume_file;
  int resume_line = -1;
  int resume_depth = 0;
  int depth = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    mode = step_mode_;
    resume_file = resume_file_;
    resume_line = resume_line_;
    resume_depth = resume_depth_;
    depth = static_cast<int>(frames_.size());
  }

  if (pause_requested_.exchange(false)) {
    stopAndWait(js, CTinyJSDebugStop::Pause, loc);
    return;
  }

  const bool same_resume =
    loc.line == resume_line && filesMatch(loc.file, resume_file) && depth == resume_depth;

  if (mode != CTinyJSDebugStep::Continue && !same_resume) {
    if (mode == CTinyJSDebugStep::In) {
      stopAndWait(js, CTinyJSDebugStop::Statement, loc);
      return;
    }
    if (mode == CTinyJSDebugStep::Over && depth <= resume_depth) {
      stopAndWait(js, CTinyJSDebugStop::Statement, loc);
      return;
    }
    if (mode == CTinyJSDebugStep::Out && depth < resume_depth) {
      stopAndWait(js, CTinyJSDebugStop::Statement, loc);
      return;
    }
  }

  if (hitBreakpoint(loc) && !same_resume)
    stopAndWait(js, CTinyJSDebugStop::Statement, loc);
}

void CTinyJSDebug::onCall(CTinyJS* js, const std::string& name, const CTinyJSDebugLoc& loc, bool native) {
  (void)js;
  if (native)
    return;
  CTinyJSDebugFrame frame;
  frame.name = name.empty() ? "<anonymous>" : name;
  frame.loc = loc;
  frame.native = false;
  std::lock_guard<std::mutex> lock(mu_);
  frames_.push_back(std::move(frame));
}

void CTinyJSDebug::onReturn(CTinyJS* js) {
  (void)js;
  std::lock_guard<std::mutex> lock(mu_);
  if (!frames_.empty())
    frames_.pop_back();
}

void CTinyJSDebug::onException(CTinyJS* js, const CTinyJSDebugLoc& loc, const std::string& message) {
  (void)message;
  stopAndWait(js, CTinyJSDebugStop::Exception, loc);
}

std::vector<CTinyJSDebugFrame> CTinyJSDebug::stack() const {
  std::lock_guard<std::mutex> lock(mu_);
  return frames_;
}

static bool SkipDebugVarName(const std::string& name) {
  if (name.empty())
    return true;
  if (name.size() >= 2 && name[0] == '_' && name[1] == '_')
    return true;
  if (name == "this" || name == "arguments" || name == "undefined" ||
      name == "NaN" || name == "Infinity" || name == "Object" ||
      name == "Array" || name == "String" || name == "Number" ||
      name == "Boolean" || name == "Function" || name == "Error" ||
      name == "EvalError" || name == "RangeError" || name == "ReferenceError" ||
      name == "SyntaxError" || name == "TypeError" || name == "Iterator" ||
      name == "StopIteration" || name == "Math" || name == "JSON" ||
      name == "Date" || name == "RegExp" || name == "eval" || name == "require" ||
      name == "parseInt" || name == "parseFloat" || name == "isNaN" ||
      name == "isFinite" || name == "charToInt" || name == "trace" ||
      name == "print" || name == "include" || name == "sleep" ||
      name == "edit" || name == "list" || name == "listLine" ||
      name == "listRange" || name == "clear" || name == "clearLine" ||
      name == "run" || name == "getTime" || name == "fileReadToEnd" ||
      name == "cmf" || name == "sys" || name == "http" || name == "env" ||
      name == "crypto" || name == "date" || name == "ApiManager")
    return true;
  return false;
}

std::vector<CTinyJSDebugVar> CTinyJSDebug::variables(CTinyJS* js, const std::string& filter) const {
  std::vector<CTinyJSDebugVar> out;
  if (!js)
    return out;

  auto consider = [&](const std::string& name, const CScriptVarPtr& var) {
    if (!var)
      return;
    if (!filter.empty() && name.find(filter) == std::string::npos)
      return;
    if (filter.empty() && SkipDebugVarName(name))
      return;
    CTinyJSDebugVar v;
    v.name = name;
    v.type = var->getVarType();
    try {
      v.value = var->toString();
      if (v.value.size() > 256)
        v.value.resize(256);
    } catch (...) {
      v.value = "<unprintable>";
    }
    if (filter.empty() && v.value.find("[native code]") != std::string::npos)
      return;
    out.push_back(std::move(v));
  };

  // Innermost scope first, then root.
  for (auto it = js->scopes.rbegin(); it != js->scopes.rend(); ++it) {
    if (!*it)
      continue;
    for (const auto& child : (*it)->Childs) {
      if (child)
        consider(child->getName(), child->getVarPtr());
    }
    if (*it == js->root)
      break;
  }
  return out;
}
