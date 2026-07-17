#!/usr/bin/env bash
# role-guard: PreToolUse kancası — aktif rol (agent_type) başına dosya-yolu
# yetkisini MEKANİK uygular. Girdi: stdin'de JSON. Çıkış 2 = tool çağrısını engelle.
#
# Roller ve sınırlar:
#   architect / project-manager : src/ altına Edit/Write yasak.
#   coder                       : yalnızca src/ ve coding.md'ye Edit/Write.
#   tester                      : src/'e her tür erişim (Read/Grep/Glob/Bash) yasak.
# Rol yoksa (ana ajan) kanca karışmaz.

input=$(cat)

agent=$(printf '%s' "$input" | jq -r '.agent_type // ""')
tool=$(printf '%s' "$input" | jq -r '.tool_name // ""')

# Ana ajan veya tanınmayan bağlam: dokunma.
[ -z "$agent" ] && exit 0

path=$(printf '%s' "$input" | jq -r '.tool_input.file_path // .tool_input.path // ""')
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')

deny() { echo "ROL-GUARD [$agent]: $1" >&2; exit 2; }

# Yol src/ altında mı? (mutlak veya göreli)
is_src() {
  case "$1" in
    src|src/*|*/src|*/src/*) return 0 ;;
    *) return 1 ;;
  esac
}

case "$agent" in
  architect|project-manager)
    case "$tool" in
      Edit|Write)
        is_src "$path" && deny "src/ altındaki C++ koduna dokunamazsın. Ne yapılacağını iletişim dosyana yaz, kodcuya bırakılsın."
        ;;
    esac
    ;;

  coder)
    case "$tool" in
      Edit|Write)
        base=$(basename "$path")
        if ! is_src "$path" && [ "$base" != "coding.md" ]; then
          deny "yalnızca src/ ve coding.md yazabilirsin. Başka dosya gerekiyorsa coding.md'ye yaz, mimara danışılsın."
        fi
        ;;
    esac
    ;;

  tester)
    # Kara-kutu: src/ testçiye tamamen kapalı (okuma dahil).
    case "$tool" in
      Read|Edit|Write)
        is_src "$path" && deny "kara-kutu testçisin; src/ içeriğini göremez/değiştiremezsin. Yalnızca wiki/examples/scripts ve resmî belgeler."
        ;;
      Grep|Glob)
        if [ -z "$path" ] || is_src "$path"; then
          deny "Grep/Glob için src DIŞI belirli bir yol ver (köke taramada src sızar). Örn. path=wiki veya path=examples."
        fi
        ;;
      Bash)
        if printf '%s' "$cmd" | grep -Eq '(^|[^A-Za-z0-9_/])src(/|$|[^A-Za-z0-9_])'; then
          deny "Bash komutun src/ referansı içeriyor; kara-kutu testçisin, kaynak koda erişemezsin."
        fi
        ;;
    esac
    ;;
esac

exit 0
