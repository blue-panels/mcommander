#! /bin/sh
#
# Fail if a commit in the given range credits an AI assistant: a
# Co-Authored-By (or similar) trailer naming one, or a "Generated with"
# footer such as Claude Code leaves.
#
# Usage: maint/check-ai-trailers.sh <rev-range>
#   e.g. maint/check-ai-trailers.sh origin/master..HEAD

set -e

if [ $# -ne 1 ]; then
    echo "usage: $0 <rev-range>" >&2
    exit 2
fi

# Names and addresses of AI assistants, matched as whole words.
ai='claude|anthropic|copilot|chatgpt|openai|codex|gemini|bard|cursor|cursoragent|aider|devin|windsurf|codeium|tabnine|sourcegraph|cody|codewhisperer|amazon q|jules|qwen|deepseek|mistral|grok|llama'

trailer="^[[:space:]]*(co-authored-by|assisted-by|generated-by|signed-off-by|helped-by):.*\\b($ai)\\b"
footer="^[^[:alnum:]]*(generated|written|created) (with|by) \\[?($ai)"

bad=0
for c in $(git rev-list "$1"); do
    hits=$(git log -1 --format=%B "$c" | grep -inE "$trailer|$footer" || true)
    if [ -n "$hits" ]; then
        echo "$(git log -1 --format='%h %s' "$c")"
        echo "$hits" | sed 's/^/    /'
        bad=1
    fi
done

if [ $bad -ne 0 ]; then
    echo >&2
    echo "Commits above credit an AI assistant; drop those lines from the messages." >&2
    exit 1
fi
