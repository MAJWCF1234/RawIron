# ri_tool functions/

Drop optional *user* recipes here (`.rifunction`, `.cmd`, `.bat`, `.ps1`).
These are **not** engine content — the engine only consumes authored assets.

Characters are modeled with a series of commands, for example:

```
ri_tool --blockchar-create my_hero --rig rigs/my_hero.ri_rig.json
ri_tool --blockchar-add-part --blockchar blockchars/my_hero.ri_blockchar.json --bone chest --shape prism --x 0 --y 1.3 --z 0.05 --sx 0.4 --sy 0.3 --sz 0.2 --color 0.4,0.5,0.3
ri_tool --blockchar-nudge --blockchar blockchars/my_hero.ri_blockchar.json --part ... --dy 0.02
ri_tool --blockchar-sync-sculpt --blockchar blockchars/my_hero.ri_blockchar.json --overwrite
```

A `.rifunction` is just those flags one per line, with `$id` / `$name` / `$overwrite` substitution:

```
--rig-create-humanoid $id --name "$name" $overwrite
--blockchar-create $id --name "$name" --rig rigs/$id.ri_rig.json $overwrite
```
