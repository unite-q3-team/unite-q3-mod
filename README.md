# baseq3a

Unofficial Quake III Arena gamecode patch

# What is done:

 * new toolchain used (optimized q3lcc and q3asm)
 * upstream security fixes
 * floatfix
 * fixed vote system
 * item replacement system (per-map) via itemreplace.txt
 * fixed spawn system
 * fixed in-game crosshair proportions
 * fixed UI mouse sensitivity for high-resolution
 * fixed server browser + faster scanning
 * fixed grappling hook muzzle position visuals
 * new demo UI (subfolders,filtering,sorting)
 * updated serverinfo UI
 * map rotation system
 * unlagged weapons
 * improved prediction
 * damage-based hitsounds
 * colored skins
 * high-quality proportional font renderer
 * single-line cvar declaration, improved cvar code readability and development efficiency
 * single-line event (EV_*) declaration
 * single-line mean of death (MOD_*) declaration

# TODO:

 * bugfixes

# Documentation

See /docs/

## Item replacement (itemreplace.txt)

Create or edit `itemreplace.txt` in the mod folder. Example:

```
# Replace all Quads on q3dm6 with MegaHealth
map.q3dm6.classmap.item_quad = item_health_mega

# Targeted rule example (coordinates are examples)
map.q3dm6.rule.r1.match.classname = item_quad
map.q3dm6.rule.r1.match.origin = 848 -456 312
map.q3dm6.rule.r1.apply.classname = item_health_mega
map.q3dm6.rule.r1.apply.origin = 860 -460 312
map.q3dm6.rule.r1.apply.angles = 0 180 0
```

Rules are applied per-map at spawn time. Targeted rules (rule.*) run before class mappings and can also remove entities via `apply.remove = 1`.

# Compilation and installation

Look in /build/
