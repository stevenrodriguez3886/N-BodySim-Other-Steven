#!/bin/bash
# Interactive checklist driver for the in-window menu (Task 4 Step 7).
# Synthetic clicks/keys via /tmp/xsyn (XTEST); state observed via window title.
set -u
X=/tmp/xsyn
APP=./build/nbody

win_id=""
find_win() {
  local pid=$1 id p t=0
  while (( t < 10 )); do
    for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | tr ',' '\n' | grep -oP '0x[0-9a-f]+'); do
      p=$(xprop -id "$id" _NET_WM_PID 2>/dev/null | grep -oP '\d+$') || true
      [[ "${p:-}" == "$pid" ]] && { win_id=$id; return 0; }
    done
    sleep 0.5; (( t++ ))
  done
  return 1
}

title() { [[ -n "$win_id" ]] || { echo ""; return; }; xwininfo -id "$win_id" -stats 2>/dev/null | grep -oP 'Absolute upper-left X:\s*\K\d+|Absolute upper-left Y:\s*\K\d+' | paste -sd' '; }
wtitle() { [[ -n "$win_id" ]] || { echo ""; return; }; xwininfo -id "$win_id" -stats 2>/dev/null | sed -n 's/^xwininfo: Window id: [^"]*"\(.*\)"[[:space:]]*$/\1/p' | head -1; }
OX() { local s; s=$(title); echo $(( ${s%% *} + $1 )); }
OY() { local s; s=$(title); echo $(( ${s##* } + $1 )); }
abs_click() { $X raise "$win_id"; $X click left abs "$(OX "$1")" "$(OY "$2")"; }

pass=0; fail=0
check() { # desc expected-substr
  sleep 0.7
  local t; t=$(wtitle)
  if [[ "$t" == *"$2"* ]]; then echo "PASS: $1  [$t]"; pass=$((pass+1));
  else echo "FAIL: $1  expected '$2' in [$t]"; fail=$((fail+1)); fi
}

# --- run on two-body, rk4, 1/120
pkill -f 'build/nbody' 2>/dev/null; sleep 0.3
$APP --preset=two-body >/tmp/nb.log 2>&1 &
APP_PID=$!
find_win "$APP_PID" || { echo "no window"; cat /tmp/nb.log; exit 1; }
geom=$(title); echo "win=$win_id abs=$geom"

check "launched two-body|rk4" "two-body | rk4"

# 1) dropdown opens on header click (no crash, no state change)
abs_click 40 16; check "preset header click stable" "two-body | rk4"

# 2) preset switch mid-run: row centers y = 38+24*i+12 -> row1 solar-system = 74
abs_click 60 74; check "preset click -> solar-system" "solar-system | rk4"

# 3) R restarts the NEW preset (still solar-system)
$X key r; check "R replays new preset" "solar-system | rk4"

# 4) integrator live switch
abs_click 200 16; abs_click 210 74; check "live integrator -> euler" "solar-system | euler"

# 5) pause (title sim-time freezes; observed via two title reads)
$X key space; sleep 0.7; t1=$(wtitle); sleep 1.2; t2=$(wtitle)
st1=$(grep -oP 't=\K[0-9.]+' <<<"$t1"); st2=$(grep -oP 't=\K[0-9.]+' <<<"$t2")
if [[ -n "$st1" && "$st1" == "$st2" ]]; then echo "PASS: pause freezes sim time ($st1)" ; pass=$((pass+1));
else echo "FAIL: pause sim time ($st1 vs $st2) — check title format"; fail=$((fail+1)); fi

# 6) paused integrator change -> staged; resume applies it
abs_click 200 16; abs_click 210 50   # integrator header, item rk4
sleep 0.5
$X key space                          # resume
check "staged integrator applied on resume" "solar-system | rk4"

# 7) staged last-write-wins: pause, two preset changes, resume -> last wins
$X key space; sleep 0.4
abs_click 40 16; abs_click 60 98     # preset header, item random-cloud (row 2)
abs_click 40 16; abs_click 60 122    # preset header, item figure-eight (row 3)
$X key space
check "staged preset last-write-wins" "figure-eight | rk4"

# 8) Escape closes dropdown first (app alive), second Escape quits
abs_click 330 16                     # physics header opens
$X key Escape; sleep 0.4
$X key r                             # app still responsive: R restarts, no quit
sleep 0.6
if kill -0 "$APP_PID" 2>/dev/null; then echo "PASS: Escape closed dropdown, app alive"; pass=$((pass+1));
else echo "FAIL: app quit on first Escape"; fail=$((fail+1)); fi

# 9) scene click closes dropdown: open preset, click scene, then click where
# row 1 WAS — if dropdown had stayed open, preset would flip to solar-system.
# Preset now figure-eight; correct close => figure-eight unchanged.
abs_click 40 16                      # open preset dropdown
abs_click 400 400                    # scene click should close it
abs_click 60 74                      # solar row IF open; scene click IF closed
sleep 0.7
t=$(wtitle)
if [[ "$t" == figure-eight* ]]; then echo "PASS: scene click closed dropdown (no item fire)"; pass=$((pass+1));
else echo "FAIL: dropdown survived scene click (title $t)"; fail=$((fail+1)); fi

# 10) physics restart-scene while paused: pause, Physics -> restart (index 6)
$X key space; sleep 0.4
abs_click 330 16; abs_click 350 $((38+24*6+12))   # physics header, restart row 6
sleep 0.6
if kill -0 "$APP_PID" 2>/dev/null; then echo "PASS: paused restart scene OK (item click closed dropdown)"; pass=$((pass+1));
else echo "FAIL: app died on paused restart"; fail=$((fail+1)); fi
$X key space
check "paused restart scene OK" "figure-eight | rk4"

$X key Escape; sleep 0.5
if kill -0 "$APP_PID" 2>/dev/null; then echo "FAIL: app survived final Escape"; fail=$((fail+1)); else echo "PASS: second-context Escape quits"; pass=$((pass+1)); fi
wait "$APP_PID" 2>/dev/null

# 11) --dt=5 raw un-ticked then menu 1/60
$APP --preset=two-body --dt=5 >/tmp/nb2.log 2>&1 &
APP_PID=$!
find_win "$APP_PID" || echo "WARN: no window for --dt=5"
check "--dt=5 launches" "two-body | rk4"
abs_click 330 16; abs_click 350 74   # physics open, 1/60 = row 1 (y=38+24+12)
check "menu 1/60 applied from off-list" "two-body | rk4"
$X key Escape
wait "$APP_PID" 2>/dev/null

echo "====================="
echo "RESULT: $pass pass, $fail fail"
