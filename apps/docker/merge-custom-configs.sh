#!/usr/bin/env bash
set -euo pipefail

target_root="$1"
custom_root="$2"

merge_config()
{
    local source_file="$1"
    local target_file="$2"
    local translate_ale="${3:-0}"

    if [[ ! -f "$source_file" ]]; then
        return
    fi

    mkdir -p "$(dirname "$target_file")"
    if [[ ! -f "$target_file" ]]; then
        if [[ -f "$target_file.dist" ]]; then
            cp "$target_file.dist" "$target_file"
        else
            touch "$target_file"
        fi
    fi

    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$translate_ale" == "1" ]]; then
            line="${line/Eluna./ALE.}"
        fi

        if [[ ! "$line" =~ ^[[:space:]]*([A-Za-z0-9_.-]+)[[:space:]]*= ]]; then
            continue
        fi

        local key="${BASH_REMATCH[1]}"
        case "$key" in
            LoginDatabaseInfo|WorldDatabaseInfo|CharacterDatabaseInfo|MySQLExecutable|DataDir|ALE.ScriptPath)
                continue
                ;;
        esac

        local temporary_file
        temporary_file="$(mktemp)"
        awk -v wanted_key="$key" -v replacement="$line" '
            BEGIN { replaced = 0 }
            {
                candidate = $0
                sub(/^[[:space:]]*/, "", candidate)
                split(candidate, parts, "=")
                parsed_key = parts[1]
                sub(/[[:space:]]*$/, "", parsed_key)
                if (!replaced && parsed_key == wanted_key)
                {
                    print replacement
                    replaced = 1
                }
                else
                    print
            }
            END {
                if (!replaced)
                    print replacement
            }
        ' "$target_file" > "$temporary_file"
        mv "$temporary_file" "$target_file"
    done < "$source_file"
}

merge_config "$custom_root/authserver.conf" "$target_root/authserver.conf"
merge_config "$custom_root/worldserver.conf" "$target_root/worldserver.conf"
merge_config "$custom_root/dbimport.conf" "$target_root/dbimport.conf"
merge_config "$custom_root/modules/instance-reset.conf" "$target_root/modules/instance-reset.conf"
merge_config "$custom_root/modules/transmog.conf" "$target_root/modules/transmog.conf"
merge_config "$custom_root/modules/mod_LuaEngine.conf" "$target_root/modules/mod_ale.conf" 1
merge_config "$custom_root/modules/mod_ale.conf" "$target_root/modules/mod_ale.conf"
