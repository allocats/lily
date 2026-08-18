# Precedence for lily

Follows C's precedence

| Category | Operators | Associativity|
| ------------- | -------------- | -------------- |
| Postfix | () [] -> . | Left to Right |
| Unary | + - ! ~ & * | Right to Left |
| Multiplicative | * / % | Left to Right |
| Additive | + -| Left to Right |
| Shift | << >> | Left to Right |
| Relational | < <= >= > | Left to Right |
| Equality | != == | Left to Right |
| Bitwise AND | & | Left to Right |
| Bitwise XOR | ^ | Left to Right |
| Bitwise OR | \| | Left to Right |
| Logical AND | && | Left to Right |
| Logical OR | \|\| | Left to Right |
| Assignment | = += -= *= /= %= <<= >>= &= ^= |= | Right to Left |
| Comma | , | Left to Right |
