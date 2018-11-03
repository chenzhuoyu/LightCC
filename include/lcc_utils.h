#ifndef LCC_PREPROCESSOR_H
#define LCC_PREPROCESSOR_H

#define _LCC_CONCAT_P(a, b) a ## b
#define _LCC_CONCAT_I(a, b) _LCC_CONCAT_P(a, b)
#define _LCC_CONCAT(a, b)   _LCC_CONCAT_I(a, b)

#define _LCC_VA_NARGS_I(                    \
    _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  \
    _9, _10, _11, _12, _13, _14, _15, _16,  \
    N, ...                                  \
) N

#define _LCC_VA_NARGS(...) _LCC_VA_NARGS_I(__VA_ARGS__, \
    16, 15, 14, 13, 12, 11, 10,  9,                     \
     8,  7,  6,  5,  4,  3,  2,  1,                     \
     0                                                  \
 )

#define _LCC_FOR_EACH_L_0(macro, data, elem, ...)
#define _LCC_FOR_EACH_L_1(macro, data, elem, ...)   macro( 0, data, elem)
#define _LCC_FOR_EACH_L_2(macro, data, elem, ...)   macro( 1, data, elem) _LCC_FOR_EACH_L_1(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_3(macro, data, elem, ...)   macro( 2, data, elem) _LCC_FOR_EACH_L_2(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_4(macro, data, elem, ...)   macro( 3, data, elem) _LCC_FOR_EACH_L_3(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_5(macro, data, elem, ...)   macro( 4, data, elem) _LCC_FOR_EACH_L_4(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_6(macro, data, elem, ...)   macro( 5, data, elem) _LCC_FOR_EACH_L_5(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_7(macro, data, elem, ...)   macro( 6, data, elem) _LCC_FOR_EACH_L_6(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_8(macro, data, elem, ...)   macro( 7, data, elem) _LCC_FOR_EACH_L_7(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_9(macro, data, elem, ...)   macro( 8, data, elem) _LCC_FOR_EACH_L_8(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_10(macro, data, elem, ...)  macro( 9, data, elem) _LCC_FOR_EACH_L_9(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_11(macro, data, elem, ...)  macro(10, data, elem) _LCC_FOR_EACH_L_10(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_12(macro, data, elem, ...)  macro(11, data, elem) _LCC_FOR_EACH_L_11(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_13(macro, data, elem, ...)  macro(12, data, elem) _LCC_FOR_EACH_L_12(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_14(macro, data, elem, ...)  macro(13, data, elem) _LCC_FOR_EACH_L_13(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_15(macro, data, elem, ...)  macro(14, data, elem) _LCC_FOR_EACH_L_14(macro, data, __VA_ARGS__)
#define _LCC_FOR_EACH_L_16(macro, data, elem, ...)  macro(15, data, elem) _LCC_FOR_EACH_L_15(macro, data, __VA_ARGS__)

#define _LCC_FOR_EACH_I(N, macro, data, elem, ...)  _LCC_CONCAT(_LCC_FOR_EACH_L_, N)(macro, data, elem, __VA_ARGS__)
#define _LCC_FOR_EACH(macro, data, ...)             _LCC_FOR_EACH_I(_LCC_VA_NARGS(__VA_ARGS__), macro, data, __VA_ARGS__)

#define _LCC_TUPLE_0_I(x, ...)  x
#define _LCC_TUPLE_1_I(x, ...)  _LCC_TUPLE_0_I(__VA_ARGS__)
#define _LCC_TUPLE_2_I(x, ...)  _LCC_TUPLE_1_I(__VA_ARGS__)
#define _LCC_TUPLE_3_I(x, ...)  _LCC_TUPLE_2_I(__VA_ARGS__)
#define _LCC_TUPLE_4_I(x, ...)  _LCC_TUPLE_3_I(__VA_ARGS__)
#define _LCC_TUPLE_5_I(x, ...)  _LCC_TUPLE_4_I(__VA_ARGS__)
#define _LCC_TUPLE_6_I(x, ...)  _LCC_TUPLE_5_I(__VA_ARGS__)
#define _LCC_TUPLE_7_I(x, ...)  _LCC_TUPLE_6_I(__VA_ARGS__)
#define _LCC_TUPLE_8_I(x, ...)  _LCC_TUPLE_7_I(__VA_ARGS__)
#define _LCC_TUPLE_9_I(x, ...)  _LCC_TUPLE_8_I(__VA_ARGS__)
#define _LCC_TUPLE_10_I(x, ...) _LCC_TUPLE_9_I(__VA_ARGS__)
#define _LCC_TUPLE_11_I(x, ...) _LCC_TUPLE_10_I(__VA_ARGS__)
#define _LCC_TUPLE_12_I(x, ...) _LCC_TUPLE_11_I(__VA_ARGS__)
#define _LCC_TUPLE_13_I(x, ...) _LCC_TUPLE_12_I(__VA_ARGS__)
#define _LCC_TUPLE_14_I(x, ...) _LCC_TUPLE_13_I(__VA_ARGS__)
#define _LCC_TUPLE_15_I(x, ...) _LCC_TUPLE_14_I(__VA_ARGS__)
#define _LCC_TUPLE_16_I(x, ...) _LCC_TUPLE_15_I(__VA_ARGS__)

#define _LCC_TUPLE_ITEM_AT_I(tuple, i)  _LCC_TUPLE_ ## i ## _I tuple
#define _LCC_TUPLE_ITEM_AT(tuple, i)    _LCC_TUPLE_ITEM_AT_I(tuple, i)

#endif /* LCC_PREPROCESSOR_H */
