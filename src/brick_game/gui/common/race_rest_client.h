#ifndef RACE_REST_CLIENT_H
#define RACE_REST_CLIENT_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// REST-клиент для гонок (общий для console/desktop).

namespace race_rest {

constexpr int RACE_FIELD_HEIGHT = 20;
constexpr int RACE_FIELD_WIDTH = 10;

struct RaceStateDto {
  bool field[RACE_FIELD_HEIGHT][RACE_FIELD_WIDTH];
  int score = 0;
  int high_score = 0;
  int level = 1;
  int speed = 0;
  bool pause = false;
  bool game_over = false;
  bool nitro = false;
  int lives = 0;
};

class RaceRestClient {
 public:
  explicit RaceRestClient(
      const std::string &base_url = default_base_url(),
      const std::string &api_prefix = default_api_prefix())
      : _base_url(strip_trailing_slash(base_url)),
        _api_prefix(normalize_prefix(api_prefix)) {}

  bool select_game(int game_id) {
    return http_post_json(url("/games/" + std::to_string(game_id)),
                          "{}");
  }

  bool send_action(int action_id, bool hold) {
    const char *h = hold ? "true" : "false";
    char body[96];
    std::snprintf(body, sizeof(body),
                  "{\"action_id\":%d,\"hold\":%s}", action_id, h);
    return http_post_json(url("/actions"), body);
  }

  bool fetch_state(RaceStateDto &out) {
    std::string json;
    if (!http_get_json(url("/state"), json)) return false;
    if (!parse_field_matrix(json, out.field)) return false;
    if (!json_get_int(json, "score", out.score)) return false;
    if (!json_get_int(json, "high_score", out.high_score)) return false;
    if (!json_get_int(json, "level", out.level)) return false;
    if (!json_get_int(json, "speed", out.speed)) return false;
    (void)json_get_bool(json, "pause", out.pause);
    (void)json_get_bool(json, "game_over", out.game_over);
    (void)json_get_bool(json, "nitro", out.nitro);
    (void)json_get_int(json, "lives", out.lives);
    return true;
  }

 private:
  std::string _base_url;
  std::string _api_prefix;

  static std::string strip_trailing_slash(std::string s) {
    while (s.size() > 1 && s.back() == '/') {
      s.pop_back();
    }
    return s;
  }

  static std::string default_base_url() {
    const char *env = std::getenv("BRICKGAME_API_BASE_URL");
    if (env == nullptr || env[0] == '\0') {
      return "http://127.0.0.1:8005";
    }
    return std::string(env);
  }

  static std::string default_api_prefix() {
    const char *env = std::getenv("BRICKGAME_API_PREFIX");
    if (env == nullptr || env[0] == '\0') return "";
    return std::string(env);
  }

  static std::string normalize_prefix(std::string prefix) {
    if (prefix.empty()) return "";
    if (prefix.front() != '/') prefix = "/" + prefix;
    prefix = strip_trailing_slash(prefix);
    return prefix;
  }

  std::string url(const std::string &path) const {
    return _base_url + _api_prefix + path;
  }

  static bool run_cmd_capture(const std::string &cmd, std::string &out) {
    out.clear();
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) return false;
    char buf[512];
    while (fgets(buf, static_cast<int>(sizeof(buf)), pipe) != nullptr) {
      out += buf;
    }
    const int rc = pclose(pipe);
    return rc == 0;
  }

  bool http_post_json(const std::string &url, const std::string &json_body) {
    const std::string cmd =
        "curl -sS -m 3 -X POST -H \"Content-Type: application/json\" -d '" +
        json_body + "' \"" + url + "\" > /dev/null";
    std::string out;
    return run_cmd_capture(cmd, out);
  }

  bool http_get_json(const std::string &url, std::string &out) {
    const std::string cmd = "curl -sS -m 3 \"" + url + "\"";
    return run_cmd_capture(cmd, out);
  }

  static bool json_get_int(const std::string &s, const char *key, int &out) {
    const std::string k = std::string("\"") + key + "\"";
    const std::size_t p = s.find(k);
    if (p == std::string::npos) return false;
    const std::size_t c = s.find(':', p + k.size());
    if (c == std::string::npos) return false;
    int v = 0;
    if (std::sscanf(s.c_str() + c + 1, " %d", &v) == 1) {
      out = v;
      return true;
    }
    return false;
  }

  static bool json_get_bool(const std::string &s, const char *key, bool &out) {
    const std::string k = std::string("\"") + key + "\"";
    const std::size_t p = s.find(k);
    if (p == std::string::npos) return false;
    const std::size_t c = s.find(':', p + k.size());
    if (c == std::string::npos) return false;
    const char *q = s.c_str() + c + 1;
    while (*q == ' ' || *q == '\t') ++q;
    if (std::strncmp(q, "true", 4) == 0) {
      out = true;
      return true;
    }
    if (std::strncmp(q, "false", 5) == 0) {
      out = false;
      return true;
    }
    return false;
  }

  static bool parse_field_matrix(const std::string &s,
                                   bool out[RACE_FIELD_HEIGHT]
                                           [RACE_FIELD_WIDTH]) {
    const std::size_t f = s.find("\"field\"");
    if (f == std::string::npos) return false;
    std::size_t p = s.find('[', f);
    if (p == std::string::npos) return false;
    p = s.find('[', p + 1);
    if (p == std::string::npos) return false;

    for (int r = 0; r < RACE_FIELD_HEIGHT; ++r) {
      if (r > 0) {
        p = s.find('[', p + 1);
        if (p == std::string::npos) return false;
      }
      for (int c = 0; c < RACE_FIELD_WIDTH; ++c) {
        p = s.find_first_not_of(" \t\r\n,", p + 1);
        if (p == std::string::npos) return false;
        if (s.compare(p, 4, "true") == 0) {
          out[r][c] = true;
          p += 3;
        } else if (s.compare(p, 5, "false") == 0) {
          out[r][c] = false;
          p += 4;
        } else {
          return false;
        }
      }
    }
    return true;
  }
};

}  // namespace race_rest

#endif  // RACE_REST_CLIENT_H

