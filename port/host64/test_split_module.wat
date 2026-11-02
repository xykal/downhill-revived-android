;; Tiny fixture for generated wasm2c hot/cold/bulk-access tests.
;; No game code or assets. 0x70e0 pages matches the game's initial wasm memory.
(module
  (memory (export "memory") 28896 28898)
  (func (export "load") (param i32) (result i32)
    local.get 0 i32.load)
  (func (export "store") (param i32 i32)
    local.get 0 local.get 1 i32.store)
  (func (export "load_offset") (param i32) (result i32)
    local.get 0 i32.load offset=1879048192)
  (func (export "fill") (param i32 i32 i32)
    local.get 0 local.get 1 local.get 2 memory.fill)
  (func (export "copy") (param i32 i32 i32)
    local.get 0 local.get 1 local.get 2 memory.copy)
)
