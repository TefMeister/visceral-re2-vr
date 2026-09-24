#!/usr/bin/env bash
# build_leon_lists.sh (2026-09-24): Leon (pl00) hold-bank lists for item 22 -- the same recipe as Claire v5 base + light v1
# (dossier 8i). Pulls the four originals from the paks into %TEMP%, splices, INSTALLS into the game and archives with hashes.
# Run from anywhere: bash dev-archive/tools/re-engine/build_leon_lists.sh   (never commit the outputs: game data)
set -u
T="$(cd "$(dirname "$0")" && pwd)"
G="/c/Steam/steamapps/common/RESIDENT EVIL 2  BIOHAZARD RE2"
S="${TEMP:-/tmp}/visceral-leon"
mkdir -p "$S"
cd "$T"
ls pak_pull.py motlist_splice.py >/dev/null || { echo "tools missing"; exit 1; }
P=natives/stm/sectionroot/animation/player/pl00/list
echo "== pull"
py pak_pull.py "$G" "$S" $P/hdg/base_hdg_hold.motlist.524 $P/hdg/base_hdg_move.motlist.524 $P/cmn/cmn_move_stlight.motlist.524 $P/hdg/hdg_hold_stlight_01.motlist.524 2>&1 | tail -4
IDLE=0160_OFF_Gazing_Idle_F_Loop
echo "== base v5"
py motlist_splice.py --character pl00 --hold "$S/$P/hdg/base_hdg_hold.motlist.524" --move "$S/$P/hdg/base_hdg_move.motlist.524" --out "$S/leon-v5" \
  --map 0140_HG_Hold_Start_L0=$IDLE@20 --map 0141_HG_Hold_Start_L90=$IDLE@22 --map 0143_HG_Hold_Start_L180=$IDLE@24 \
  --map 0150_HG_Hold_Start_R0=$IDLE@20 --map 0151_HG_Hold_Start_R90=$IDLE@22 --map 0153_HG_Hold_Start_R180=$IDLE@24 2>&1 | grep -E "verify|wrote|error|Error"
OL=0160_OLF_Gazing_Idle_F_Loop
echo "== light v1"
py motlist_splice.py --character pl00 --hold "$S/$P/hdg/hdg_hold_stlight_01.motlist.524" --move "$S/$P/cmn/cmn_move_stlight.motlist.524" --out "$S/leon-light-v1" \
  --fill 0x6e=0190_OLF_GazingWalk_F_Loop --fill 0x6f=0191_OLF_GazingWalk_L_Loop --fill 0x70=0194_OLF_GazingWalk_R_Loop --fill 0x71=0196_OLF_GazingWalk_Back_L_Loop --fill 0x72=0197_OLF_GazingWalk_Back_B_Loop --fill 0x73=0198_OLF_GazingWalk_Back_R_Loop \
  --fill 0x78=0190_OLF_GazingWalk_F_Loop --fill 0x7a=0191_OLF_GazingWalk_L_Loop --fill 0x7c=0197_OLF_GazingWalk_Back_B_Loop --fill 0x7e=0194_OLF_GazingWalk_R_Loop --fill 0x84=0191_OLF_GazingWalk_L_Loop --fill 0x88=0194_OLF_GazingWalk_R_Loop \
  --fill 0xa0=$OL --fill 0xa6=$OL --fill 0x8c=$OL@20 --fill 0x8d=$OL@22 --fill 0x8f=$OL@24 --fill 0x96=$OL@20 --fill 0x97=$OL@22 --fill 0x99=$OL@24 2>&1 | grep -E "verify|wrote|error|Error"
B="$S/leon-v5/$P/hdg/base_hdg_hold.motlist.524"; L="$S/leon-light-v1/$P/hdg/base_hdg_hold.motlist.524"
ls -l "$B" "$L" || exit 1
D="$G/natives/STM/SectionRoot/Animation/player/pl00/list/hdg"
mkdir -p "$D"
cp "$B" "$D/base_hdg_hold.motlist.524"; cp "$L" "$D/hdg_hold_stlight_01.motlist.524"
A="/d/RE2 REFramework builds/extracted (game data - never commit)/splice-archive/leon-v5-and-light-v1"
mkdir -p "$A"; cp "$D/base_hdg_hold.motlist.524" "$A/pl00_base_hdg_hold.v5.motlist.524"; cp "$D/hdg_hold_stlight_01.motlist.524" "$A/pl00_hdg_hold_stlight_01.v1.motlist.524"
(cd "$A" && sha256sum *.524 | tee MANIFEST.sha256)
echo "== installed"; ls -l "$D"
