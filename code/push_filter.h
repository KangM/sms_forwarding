#pragma once

struct PushFilterRule {
  bool enabled;
  PushFilterTarget target;
  PushFilterMode mode;
  String expr;
};

struct PushFilterEvalResult {
  bool valid;
  bool allowed;
  String reason;
  String targetName;
  String modeName;
  String expr;
};

String pushFilterTargetName(PushFilterTarget target) {
  return target == PUSH_FILTER_TARGET_SENDER ? "sender" : "message";
}

String pushFilterModeName(PushFilterMode mode) {
  switch (mode) {
    case PUSH_FILTER_MODE_CONTAINS: return "contains";
    case PUSH_FILTER_MODE_NOT_CONTAINS: return "not_contains";
    case PUSH_FILTER_MODE_STARTS_WITH: return "starts_with";
    case PUSH_FILTER_MODE_ENDS_WITH: return "ends_with";
    default: return "unknown";
  }
}

PushFilterRule currentPushFilterRule() {
  PushFilterRule rule;
  rule.enabled = config.pushFilterEnabled;
  rule.target = config.pushFilterTarget;
  rule.mode = config.pushFilterMode;
  rule.expr = config.pushFilterExpr;
  return rule;
}

bool pushFilterMatchToken(const String& targetText, PushFilterMode mode, const String& token) {
  switch (mode) {
    case PUSH_FILTER_MODE_CONTAINS:
      return targetText.indexOf(token) >= 0;
    case PUSH_FILTER_MODE_NOT_CONTAINS:
      return targetText.indexOf(token) < 0;
    case PUSH_FILTER_MODE_STARTS_WITH:
      return targetText.startsWith(token);
    case PUSH_FILTER_MODE_ENDS_WITH:
      return targetText.endsWith(token);
    default:
      return false;
  }
}

PushFilterEvalResult evaluatePushFilter(const PushFilterRule& rule, const String& sender, const String& message) {
  PushFilterEvalResult result;
  result.valid = true;
  result.allowed = true;
  result.targetName = pushFilterTargetName(rule.target);
  result.modeName = pushFilterModeName(rule.mode);
  result.expr = rule.expr;
  result.expr.trim();

  if (!rule.enabled) {
    result.reason = "filter disabled, allowed";
    return result;
  }
  if (result.expr.length() == 0) {
    result.reason = "empty filter expression, allowed";
    return result;
  }

  bool hasAnd = result.expr.indexOf("&&") >= 0;
  bool hasOr = result.expr.indexOf("||") >= 0;
  if (hasAnd && hasOr) {
    result.valid = false;
    result.allowed = false;
    result.reason = "invalid rule: do not mix && and ||";
    return result;
  }

  String targetText = rule.target == PUSH_FILTER_TARGET_SENDER ? sender : message;
  String delimiter = hasAnd ? "&&" : (hasOr ? "||" : "");
  bool finalMatch = hasAnd;
  if (!hasAnd && !hasOr) finalMatch = false;

  int start = 0;
  while (start <= result.expr.length()) {
    int end = delimiter.length() > 0 ? result.expr.indexOf(delimiter, start) : -1;
    String token = end >= 0 ? result.expr.substring(start, end) : result.expr.substring(start);
    token.trim();
    if (token.length() == 0) {
      result.valid = false;
      result.allowed = false;
      result.reason = "invalid rule: empty condition";
      return result;
    }

    bool matched = pushFilterMatchToken(targetText, rule.mode, token);
    if (hasAnd) {
      finalMatch = finalMatch && matched;
    } else if (hasOr) {
      finalMatch = finalMatch || matched;
    } else {
      finalMatch = matched;
    }

    if (end < 0) break;
    start = end + delimiter.length();
  }

  result.allowed = finalMatch;
  result.reason = finalMatch ? "matched, allowed" : "not matched, blocked";
  return result;
}

