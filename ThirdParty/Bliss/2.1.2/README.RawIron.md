# Bliss 2.1.2 source for Serenity

The `shaders` directory is a byte-for-byte copy of the supplied local
`Bliss_v2.1.2_(Chocapic13_Shaders_edit)/shaders` pack. It includes the original
noise assets, settings, translations, dimension programs, and attribution comments.
Bliss/Chocapic13 and the individual credited shader authors retain their authorship;
Serenity is RawIron's adapter name, not a claim that RawIron authored this pack.

`port-manifest.json` records all 344 source-file hashes, settings definitions,
native color-stage files, and missing renderer inputs. This pack is reference
only: the engine does not compile it. `Scripts/Generate-SerenityAdapter.py`
copies the color-transform and dither libs into
`Source/RawIron.Render.Vulkan/shaders`, rebuilds `SerenityComposite.frag`, and
refreshes this catalog. `--check` verifies generated engine files still match
this frozen source.

The native implementation ports color output, scene-linear exposure metering,
and rod response, with engine approximations for bloom, temporal stabilization,
clouds, water, and indirect light. It does not execute the Minecraft/Iris
pipeline and does not yet match its full appearance. See
`docs/SERENITY_PROCESSING_STACK.md` for the remaining inputs and differences.
