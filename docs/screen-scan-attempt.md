# Screen rack scan attempt (archived)

**Status:** Abandoned as unreliable (2026-11-15).  
**Baseline restored to:** git commit `7e9df7e` — *“Restore point before the screen rack scan.”*  
**Related captures:** `Capture/2026-11-07_17-37/` (left on disk; not ignored at the restore-point commit — keep or delete locally).

This note records what was built for EverQuest weapon-rack **screen scanning**, what worked, what failed, and the best next direction if the feature is revisited.

---

## Goal

Replace or supplement log-based rack entry with a live (or capture-folder) screen scan that:

1. Reads facing from the EQ compass (East → racks 1–2, West → racks 3–4).
2. Finds the two wall weapon plaques in view.
3. Classifies each of four slots as sword / spear / axe / empty.
4. Writes clear racks onto the solver board (and optionally auto-solves).

Hotkey / UI pieces that were wired: F18 scan, Settings compass rect + session length, “Last Puzzle”, “Test capture folder…”.

---

## Architecture that was implemented (then removed)

| Piece | Role |
|--------|------|
| `src/scan.c` / `scan.h` | Capture (BitBlt, monitor scoped to compass), WIC PNG load/save, facing, plaque find, slot classify, annotate, Capture session folders |
| `src/diag_capture.exe` | Offline PNG → facing/slots + `*_read.png` / `*.diag.png` |
| `src/test_scan.c` | Unit tests + capture ground-truth regression |
| `src/app.c` integration | Apply scan → board; Settings UI; hotkey; test-folder browse |

**Ground truth used for refinement** (`Capture/2026-11-07_17-37/`):

| File | Facing | Expected |
|------|--------|----------|
| `17-37-56.png` | East | R1: Sw Sp Ax Empty · R2: Sp Ax Empty Empty |
| `17-38-00.png` | West | R3: Sw Sp Ax Empty · R4: Sw Ax Sw Sp |

---

## What worked

1. **Monitor-scoped capture**  
   Capturing only the monitor that contains the compass rect avoided multi-monitor coordinate bugs.

2. **Capture session folders**  
   Creating the Capture directory only when a pair is saved (not on session “ensure”) stopped empty folder spam.

3. **Compass facing on the labeled pair**  
   Glyph thirds + West bottom-corner scoring eventually read `17-37-56` as East and `17-38-00` as West (earlier: West often → South).

4. **Wall plaque search window**  
   Extending wall X search past ~70% width (to ~82%) was required so the West **right** rack (bright metal ~x 1320–1400 on 1920) was not clipped.

5. **East slot labels (lab pair)**  
   After many iterations, East matched ground truth exactly and stayed green under regression while West was still being tuned.

6. **West slot labels (lab pair only, fragile)**  
   Briefly both West racks matched the labels above via:
   - dim-steel vs tip-spear ordering,
   - cyan combat-text rejection,
   - empty-slot “one-sided bloom” filter,
   - a **pattern-specific** post-pass: when fixed centers yielded `Sw Ax Ax Empty`, re-probe lower peaks and force `Sw Sp` on slots 2–3 if the bottom peak looked tip-clustered.

7. **Green / torch separation (partial)**  
   Lime + strict cyan combat-text filters helped empty slots; over-broad cyan rules ate real axe glow and had to be tightened again.

8. **Close-up Example photos**  
   Orginal / Swords / Spears / Blank majority-class tests could stay green if wall-only heuristics were gated carefully.

9. **Product UX bits**  
   “Scan Recent” → “Last Puzzle”; Settings “Test capture folder…”; session mkdir deferred to save — these were solid regardless of classifier quality.

---

## What did not work (or only worked in the lab)

1. **Reliability outside the two labeled frames**  
   User report (2026-11-15): overall screen scan was **not reliable** in real use. Lab success did not transfer.

2. **West right rack discovery**  
   Farther rack often had **no** `is_weapon_pixel` hits under pale/icy thresholds (dimmer metal). Finder depended on bright axes; dim swords/spears were invisible to the column-peak pass.

3. **Facing misreads**  
   Drawn/real compass glyphs: W vs S confusion (strong mid column), E vs others without careful thirds / center-tick windowing. Softened unit tests hid regressions.

4. **Slot classification brittleness**  
   Same heuristics fought each other:
   - Dim-steel helped distant swords but flooded stone → false swords / wrong alignment.
   - `bright ≥ 80 → axe` crushed thin horizontal axe blades *and* bright swords.
   - Tip-cluster spear rules stole dim swords (edge noise).
   - Empty bloom filters stole real one-sided axe heads if too aggressive.
   - Content-span / peak-based vertical refit fixed West R4 momentarily then **broke East**.

5. **Vertical box / center coupling**  
   `tighten_plaque_box` to bright metal cropped dim top swords (West R4). Padding upward pulled stone noise and shifted centers. Fixed fractional centers (`0.14…0.84`) were wrong when the finder box was tall relative to the weapon stack.

6. **Hacky West R4 post-pass**  
   Pattern match `Sw Ax Ax Empty` → force `Sw Sp` was tuned to one screenshot. That is not a general model of EQ weapon glow.

7. **Solve after “Test capture folder”**  
   Detection could update the board while Solve stayed idle: needs **Auto solve** + exact **4/4/4** weapon counts. Wrong West slots → incomplete totals → no solve (easy to misread as “scan broken”).

8. **“Auto codes” checkbox vanishing**  
   Layout clipped the third checkbox when the window was narrow (especially with “Last Puzzle” visible). Separate from classifier quality.

9. **Green-text false negatives**  
   EQ floating text is often cyan (`g` and `b` high). Classic “lime only” green checks missed it; broader checks damaged axes.

---

## Techniques tried (inventory)

- Color gates: pale metal, icy blue, dim steel, wood/peg, torch-hot, green/cyan text  
- Wall vs close-up plaque passes; structure score; sibling-peak hunt  
- Compass: bright tape, center tick, glyph thirds, W corner boost, S veto when both feet lit  
- Slot rows: cover, half_w, tip thirds, empty thresholds, row_small vs close-up branches  
- Vertical: tighten to pale, height extend for empty slots, content-span quartering, peak snap, leading-empty compact, Sw/Ax/Ax/Empty lower-band rewrite  
- Tooling: `diag_capture`, Capture sessions, annotated overlays, capture ground-truth tests  

---

## Recommended next course of action (if revisited)

Prioritize **robustness over fitting two PNGs**.

1. **Separate “find rack” from “read slots”**  
   Prefer wood/plaque geometry (or a small template) over weapon glow for localization so dim racks still get a box.

2. **Calibrate colors from labeled crops, not globals**  
   Build a tiny per-weapon / empty / torch / text palette from many captures (East+West, lighting, distance). Gate dim-steel only inside a plaque ROI.

3. **Slot banding from content peaks inside the plaque**  
   Detect 1–4 horizontal metal bands; map to four slots by order; empties = missing bands. Avoid fixed fractions of an oversized finder box.

4. **Drop one-off pattern rewrites**  
   Replace the `Sw Ax Ax Empty → Sw Ax Sw Sp` patch with band-based logic that works for both 2-weapon and 4-weapon racks.

5. **Facing**  
   Either OCR the center glyph with a tiny template set (N/E/S/W) or require the user to confirm facing when confidence is low.

6. **Eval set before shipping**  
   ≥10 East and ≥10 West captures with labels; CI must pass all. Do not treat a single folder as done.

7. **Product**  
   Keep Capture/test-folder workflow; when auto-solve fails after a test folder, surface **why** (counts not 4/4/4 / facing / unclear slots). Fix Auto codes layout clipping independently.

8. **Practical interim**  
   Log-monitor + manual entry remain the reliable path; screen scan as optional assist with confidence flags, not silent board overwrite.

---

## Restore / delete checklist (done when this doc was written)

- Restored to `7e9df7e` working tree for: `.gitignore`, `build.bat`, `src/app.c`
- Removed untracked scan sources: `src/scan.c`, `src/scan.h`, `src/test_scan.c`, `src/diag_capture.c`
- Left `Capture/` and `Examples/Compass/` on disk for possible future labeling (untracked)
- Removed `Examples/Rack Images/*.diag.png` overlays

To restore this baseline again later:

```bat
git checkout 7e9df7e -- .gitignore build.bat src/app.c
```

and ensure scan sources are not linked from `build.bat`.
