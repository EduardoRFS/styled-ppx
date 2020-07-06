open Migrate_parsetree;
open Ast_410;
open Ast_helper;
open Reason_css_parser;
open Parser;

let (let.ok) = Result.bind;

exception Unsupported_feature;

let id = Fun.id;

type transform('ast, 'value) = {
  ast_of_string: string => result('ast, string),
  value_of_ast: 'ast => 'value,
  value_to_expr: 'value => list(Parsetree.expression),
  ast_to_expr: 'ast => list(Parsetree.expression),
  string_to_expr: string => result(list(Parsetree.expression), string),
};

let transform = (parser, value_of_ast, value_to_expr) => {
  let ast_of_string = Parser.parse(parser);
  let ast_to_expr = ast => value_of_ast(ast) |> value_to_expr;
  let string_to_expr = string =>
    ast_of_string(string) |> Result.map(ast_to_expr);
  {ast_of_string, value_of_ast, value_to_expr, ast_to_expr, string_to_expr};
};

let render_css_wide_keywords = (name, value) => {
  let.ok value = Parser.parse(Standard.css_wide_keywords, value);
  let value =
    switch (value) {
    | `Inherit =>
      %expr
      "inherit"
    | `Initial =>
      %expr
      "initial"
    | `Unset =>
      %expr
      "unset"
    };
  let name = Const.string(name) |> Exp.constant;
  Ok([[%expr Css.unsafe([%e name], [%e value])]]);
};
let render_integer = integer => Const.int(integer) |> Exp.constant;
let render_number = number =>
  Const.float(number |> string_of_float) |> Exp.constant;

let variants_to_expression =
  fun
  | `Row => id([%expr `row])
  | `Row_reverse => id([%expr `rowReverse])
  | `Column => id([%expr `column])
  | `Column_reverse => id([%expr `columnReverse])
  | `Nowrap => id([%expr `nowrap])
  | `Wrap => id([%expr `wrap])
  | `Wrap_reverse => id([%expr `wrapReverse])
  | `Content => id([%expr `content])
  | `Flex_start => id([%expr `flexStart])
  | `Flex_end => id([%expr `flexEnd])
  | `Center => id([%expr `center])
  | `Space_between => id([%expr `spaceBetween])
  | `Space_around => id([%expr `spaceAround])
  | `Baseline => id([%expr `baseline])
  | `Stretch => id([%expr `stretch])
  | `Auto => id([%expr `auto])
  | `None => id([%expr `none])
  | `Content_box => id([%expr `contentBox])
  | `Border_box => id([%expr `borderBox]);

let apply = (parser, map, id) =>
  transform(parser, map, arg => [[%expr [%e id]([%e arg])]]);
let variants = (parser, identifier) =>
  apply(parser, variants_to_expression, identifier);

// TODO: all of them could be float, but bs-css doesn't support it
let render_length =
  fun
  | `Cap(_n) => raise(Unsupported_feature)
  | `Ch(n) => [%expr `ch([%e render_number(n)])]
  | `Cm(n) => [%expr `cm([%e render_number(n)])]
  | `Em(n) => [%expr `em([%e render_number(n)])]
  | `Ex(n) => [%expr `ex([%e render_number(n)])]
  | `Ic(_n) => raise(Unsupported_feature)
  | `In(_n) => raise(Unsupported_feature)
  | `Lh(_n) => raise(Unsupported_feature)
  | `Mm(n) => [%expr `mm([%e render_number(n)])]
  | `Pc(n) => [%expr `pc([%e render_number(n)])]
  | `Pt(n) => [%expr `pt([%e render_integer(n |> int_of_float)])]
  | `Px(n) => [%expr `pxFloat([%e render_number(n)])]
  | `Q(_n) => raise(Unsupported_feature)
  | `Rem(n) => [%expr `rem([%e render_number(n)])]
  | `Rlh(_n) => raise(Unsupported_feature)
  | `Vb(_n) => raise(Unsupported_feature)
  | `Vh(n) => [%expr `vh([%e render_number(n)])]
  | `Vi(_n) => raise(Unsupported_feature)
  | `Vmax(n) => [%expr `vmax([%e render_number(n)])]
  | `Vmin(n) => [%expr `vmin([%e render_number(n)])]
  | `Vw(n) => [%expr `vw([%e render_number(n)])]
  | `Zero => [%expr `zero];
let render_percentage = render_number;

let render_length_percentage =
  fun
  | `Length(length) => render_length(length)
  | `Percentage(percentage) => [%expr
      `percent([%e render_percentage(percentage)])
    ];

// css-sizing-3
let render_function_fit_content = _lp => raise(Unsupported_feature);
let width =
  apply(
    property_width,
    fun
    | `Auto => variants_to_expression(`Auto)
    | `Length_percentage(lp) => render_length_percentage(lp)
    | `Max_content
    | `Min_content => raise(Unsupported_feature)
    | `Fit_content(lp) => render_function_fit_content(lp),
    [%expr Css.width],
  );
let height = apply(property_height, width.value_of_ast, [%expr Css.height]);
let min_width =
  apply(property_min_width, width.value_of_ast, [%expr Css.minWidth]);
let min_height =
  apply(property_min_height, width.value_of_ast, [%expr Css.minHeight]);
let max_width =
  apply(
    property_max_width,
    fun
    | `None => variants_to_expression(`None)
    | `Length_percentage(lp) => width.value_of_ast(`Length_percentage(lp))
    | `Max_content => width.value_of_ast(`Max_content)
    | `Min_content => width.value_of_ast(`Min_content)
    | `Fit_content(lp) => width.value_of_ast(`Fit_content(lp)),
    [%expr Css.maxWidth],
  );
let max_height =
  apply(property_max_height, max_width.value_of_ast, [%expr Css.maxHeight]);
let box_sizing =
  apply(property_box_sizing, variants_to_expression, [%expr Css.boxSizing]);
// TODO: bs-css doesn't support columnWidth
// let column_width =

// css-flexbox-1
// using id() because refmt
let flex_direction =
  variants(property_flex_direction, [%expr Css.flexDirection]);
let flex_wrap = variants(property_flex_wrap, [%expr Css.flexWrap]);
// shorthand - https://drafts.csswg.org/css-flexbox-1/#flex-flow-property
let flex_flow =
  transform(
    property_flex_flow,
    id,
    ((direction_ast, wrap_ast)) => {
      let direction = Option.map(flex_direction.ast_to_expr, direction_ast);
      let wrap = Option.map(flex_wrap.ast_to_expr, wrap_ast);
      [direction, wrap] |> List.concat_map(Option.value(~default=[]));
    },
  );
// TODO: this is safe?
let order = apply(property_order, render_integer, [%expr Css.order]);
let flex_grow =
  apply(property_flex_grow, render_number, [%expr Css.flexGrow]);
let flex_shrink =
  apply(property_flex_shrink, render_number, [%expr Css.flexShrink]);
// TODO: is safe to just return Css.width when flex_basis?
let flex_basis =
  apply(
    property_flex_basis,
    fun
    | `Content => variants_to_expression(`Content)
    | `Property_width(value_width) => width.value_of_ast(value_width),
    [%expr Css.flexBasis],
  );
// TODO: this is incomplete
let flex =
  transform(
    property_flex,
    id,
    fun
    | `None => [[%expr Css.flex(`none)]]
    | `Or(grow_shrink, basis) => {
        let grow_shrink =
          switch (grow_shrink) {
          | None => []
          | Some((grow, shrink)) =>
            List.concat([
              flex_grow.ast_to_expr(grow),
              Option.map(flex_shrink.ast_to_expr, shrink)
              |> Option.value(~default=[]),
            ])
          };
        let basis =
          switch (basis) {
          | None => []
          | Some(basis) => flex_basis.ast_to_expr(basis)
          };
        List.concat([grow_shrink, basis]);
      },
  );
// TODO: justify_content, align_items, align_self, align_content are only for flex, missing the css-align-3 at parser
let justify_content =
  variants(property_justify_content, [%expr Css.justifyContent]);
let align_items = variants(property_align_items, [%expr Css.alignItems]);
let align_self = variants(property_align_self, [%expr Css.alignSelf]);
let align_content =
  variants(property_align_content, [%expr Css.alignContent]);

let found = ({string_to_expr, _}) => string_to_expr;
let properties = [
  // css-sizing-3
  ("width", found(width)),
  ("height", found(height)),
  ("min-width", found(min_width)),
  ("min-height", found(min_height)),
  ("max-width", found(max_width)),
  ("max-height", found(max_height)),
  ("box-sizing", found(box_sizing)),
  // css-flexbox-1
  ("flex-direction", found(flex_direction)),
  ("flex-wrap", found(flex_wrap)),
  ("flex-flow", found(flex_flow)),
  ("order", found(order)),
  ("flex-grow", found(flex_grow)),
  ("flex-shrink", found(flex_shrink)),
  ("flex-basis", found(flex_basis)),
  // TODO: missing a proper implementation
  ("flex", found(flex)),
  ("justify-content", found(justify_content)),
  ("align-items", found(align_items)),
  ("align-self", found(align_self)),
  ("align-content", found(align_content)),
];

let support_property = name =>
  properties |> List.exists(((key, _)) => key == name);
let parse_declarations = ((name, value)) => {
  let (let.ok) = Result.bind;
  let.ok (_, string_to_expr) =
    properties
    |> List.find_opt(((key, _)) => key == name)
    |> Option.to_result(~none=`Not_found);
  switch (render_css_wide_keywords(name, value)) {
  | Ok(value) => Ok(value)
  | Error(_) =>
    string_to_expr(value) |> Result.map_error(str => `Invalid_value(str))
  };
};
