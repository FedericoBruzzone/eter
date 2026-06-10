# Items
## Modules
## Constant 
## External block
## Use declarations
## Functions
## Structs

A struct declaration defines a new product type with a name and zero or more fields. Fields are comma-separated (a trailing comma is allowed) and each field may optionally specify a regime — `imm`, `mut`, or `proj` — before its name (see the [Memory Model Chapter](./memory-model.md)).

```rust
struct Point {
    x: i32,
    y: i32,
}

struct Buffer {
    imm len: i32,
    mut data: f32,
}

struct Marker {}   // Unit-like struct
```

See the [Type System Chapter](./type-system.md#struct-types) for struct semantics and instantiation.

## Enumerations

An enum declaration defines a new enumerated type with a name and zero or more comma-separated variants. A variant comes in one of three forms:

- **Unit variant**: just a name, with an optional explicit discriminant (`Name = ConstExpr`);
- **Tuple variant**: a name followed by a parenthesised list of types;
- **Struct variant**: a name followed by a braced list of struct fields.

```rust
enum Animals {
    Dog(str, i32),                  // Tuple variant
    Cat { name: str, age: i32 },    // Struct variant
    Reptile,                        // Unit variant
}

enum Balls {
    Tennis,      // Discriminant 0 (implicit)
    Golf = 10,   // Explicit discriminant
    Soccer,      // Discriminant 11 (previous + 1)
}
```

See the [Type System Chapter](./type-system.md#enum-types) for enum semantics and discriminant rules.

## Unions

A union declaration defines a type whose fields can store different types of values, but only one at a time. The body uses the same field syntax as structs.

```rust
union MyUnion {
    f1: u32,
    f2: f32,
}
```

See the [Type System Chapter](./type-system.md#union-types) for union semantics.
