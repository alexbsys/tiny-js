#ifndef TINYJS_DEBUG_H
#define TINYJS_DEBUG_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

class CTinyJS;

enum class CTinyJSDebugStop {
  None = 0,
  Statement,
  Call,
  Return,
  Exception,
  Pause
};

enum class CTinyJSDebugStep {
  Continue = 0,
  In,
  Over,
  Out
};

struct CTinyJSDebugLoc {
  std::string file;
  int line = -1;      // 1-based (editor / DAP)
  int column = -1;    // 1-based
  int end_column = -1;
};

struct CTinyJSDebugFrame {
  std::string name;
  CTinyJSDebugLoc loc;
  bool native = false;
};

struct CTinyJSDebugVar {
  std::string name;
  std::string value;
  std::string type;
};

typedef void (*CTinyJSDebugHook)(CTinyJS* js, CTinyJSDebugStop stop,
                                 const CTinyJSDebugLoc& loc, void* user);

class CTinyJSDebug {
 public:
  CTinyJSDebug();
  ~CTinyJSDebug();

  void setEnabled(bool on);
  bool isEnabled() const { return enabled_; }

  void setHook(CTinyJSDebugHook hook, void* user);
  void addBreakpoint(const std::string& file, int line_1based);
  void removeBreakpoint(const std::string& file, int line_1based);
  void clearBreakpoints();

  void requestPause();
  void debugContinue();
  void debugStepIn();
  void debugStepOver();
  void debugStepOut();

  bool isPaused() const { return paused_; }
  CTinyJSDebugStop lastStop() const { return last_stop_; }
  CTinyJSDebugLoc lastLoc() const { return last_loc_; }

  std::vector<CTinyJSDebugFrame> stack() const;
  std::vector<CTinyJSDebugVar> variables(CTinyJS* js, const std::string& filter = std::string()) const;

  // Interpreter (only when CTinyJS::debug_enabled_ is true).
  void onStatement(CTinyJS* js);
  void onCall(CTinyJS* js, const std::string& name, const CTinyJSDebugLoc& loc, bool native);
  void onReturn(CTinyJS* js);
  void onException(CTinyJS* js, const CTinyJSDebugLoc& loc, const std::string& message);

  static bool filesMatch(const std::string& a, const std::string& b);
  static CTinyJSDebugLoc locFromTokenizer(CTinyJS* js);

 private:
  void stopAndWait(CTinyJS* js, CTinyJSDebugStop reason, const CTinyJSDebugLoc& loc);
  bool hitBreakpoint(const CTinyJSDebugLoc& loc) const;
  void resume(CTinyJSDebugStep next);

  bool enabled_ = false;
  CTinyJSDebugHook hook_ = nullptr;
  void* hook_user_ = nullptr;

  std::atomic<bool> pause_requested_{false};
  std::atomic<bool> paused_{false};
  CTinyJSDebugStep step_mode_ = CTinyJSDebugStep::Continue;
  CTinyJSDebugStop last_stop_ = CTinyJSDebugStop::None;
  CTinyJSDebugLoc last_loc_;
  std::string resume_file_;
  int resume_line_ = -1;
  int resume_depth_ = 0;

  struct Breakpoint {
    std::string file;
    int line = -1;
  };
  std::vector<Breakpoint> breakpoints_;
  std::vector<CTinyJSDebugFrame> frames_;

  mutable std::mutex mu_;
  std::condition_variable cv_;
};

#endif
