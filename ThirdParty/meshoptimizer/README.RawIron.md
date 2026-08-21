# meshoptimizer

Raw Iron vendors the decoder subset of meshoptimizer **v0.25**, pinned to commit
`6daea4695c48338363b08022d2fb15deaef6ac09` from
<https://github.com/zeux/meshoptimizer>.

It is used privately by `RawIron.SceneUtilities` to decode glTF
`EXT_meshopt_compression` buffer views. The upstream MIT license is preserved in
`LICENSE.md`. Keeping only the source/header files required by the importer avoids a
network-dependent build and avoids shipping the upstream repository history, demos,
tests, or tools.
