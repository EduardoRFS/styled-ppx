open Migrate_parsetree;
open Ast_406;
open Ast_mapper;
open Asttypes;
open Parsetree;

open Ast_helper;
open Longident;

/* let expr = (mapper, expression) =>
  switch (expression.pexp_desc) {
    /* module_expr */

    )) when txt == "styled" =>
      switch (e.pexp_desc) {
      | Pexp_constant(Pconst_string(str, delim)) =>
        let loc_start =
          switch (delim) {
          | None => e.pexp_loc.Location.loc_start
          | Some(s) => {
              ...e.pexp_loc.Location.loc_start,
              Lexing.pos_cnum:
                e.pexp_loc.Location.loc_start.Lexing.pos_cnum
                + String.length(s)
                + 1,
            }
          };

          let ast =
            Css_lexer.parse_string(
              ~container_lnum=loc_start.Lexing.pos_lnum,
              ~pos=loc_start,
              str,
              Css_parser.declaration_list,
            );

          Css_to_emotion.render_declaration_list(ast);
      | _ =>
        let message =
          switch (txt) {
          | "styled" => "[%style] accepts a string, e.g. [%style \"color: red;\"]"
          | _ => assert(false)
        };
        raise(Location.Error(Location.error(~loc, message)));
    }
  | _ => default_mapper.expr(mapper, expression)
  }; */

/* structure_item: (mapper, item) => {
  switch (item.pstr_desc) {
    | Pstr_value(
        Nonrecursive,
        [
          {
            pvb_pat: {ppat_desc: Ppat_var(className), _},
            pvb_expr: {
              pexp_desc:
                Pexp_extension((
                  {txt: "styled", _},
                  PStr([
                    {
                      pstr_desc:
                        Pstr_eval(
                          {
                            pexp_desc:
                              Pexp_construct({txt: Lident("[]"), _}, None), _
                          },
                          [],
                        ),
                    _},
                  ]),
                )),
            _},
          _},
        ],
      ) =>
      Str.value(
        Nonrecursive,
        [Vb.mk(Pat.var(className), css(className, EmptyList))],
      )
    | _ => default_mapper.structure_item(mapper, item)
  }
}, */

/*
| Pexp_extension((
      {txt, loc, _},
      PStr([{
        pstr_desc:
          Pstr_module({
            pmb_expr: {
              pmod_desc: Pmod_structure([{
                /* pstr_desc: Pstr_eval(e, _) */
                pstr_desc:
                  Pstr_primitive(
                    {pval_name: {txt: fnName, _}, pval_attributes, pval_type, _}
                  )
              , _}]),
            _},
          _}),
        _}]),
 */

let createStyles = (loc, name, exp) => {
  let variableName = {
    ppat_desc: Ppat_var({ txt: name, loc}),
    ppat_loc: loc,
    ppat_attributes: [],
  };

  Str.mk(
    ~loc,
    Pstr_value(
      Nonrecursive,
      [Vb.mk(~loc, variableName, exp)],
    )
  )
};

let createReactComponent = (~loc) =>
  Str.mk(
    ~loc,
    Pstr_value(
      Nonrecursive,
      [
        Vb.mk(
          ~loc,
          ~attrs=[({ txt: "react.component", loc }, PStr([]))],
          Pat.mk(~loc,
            Ppat_var({ txt: "make", loc }),
          ),
          Exp.fun_(
            ~loc,
            Labelled("children"),
            None,
            Pat.mk(~loc, Ppat_var({txt: "children", loc })),
            Exp.apply(
              ~loc,
              Exp.ident({ txt: Lident("div"), loc }),
              []
            )
          )
        )
      ]
    )
  );

let createModule = (~loc, ~ast) =>
  Mod.mk(
    Pmod_structure([
      createStyles(loc, "styled", Css_to_emotion.render_declaration_list(ast)),
      createReactComponent(~loc),
    ]),
  );

let moduleMapper = (_, _) => {
  ...default_mapper,
  /* We map all the modules */
  module_expr: (mapper, expr) =>
    switch (expr) {
    | {
        pmod_desc:
          /* that are defined with the ppx [%styled] */
          Pmod_extension((
            { txt: "styled", loc },
            PStr([
              {
                /* and contains a string */
                pstr_desc:
                  Pstr_eval(
                    { pexp_desc: Pexp_constant(Pconst_string(str, delim)), _ },
                    _
                  ),
                _
              },
            ]),
          )),
        _
      } =>
      {
        let loc_start = switch (delim) {
          | None => loc.Location.loc_start
          | Some(s) => {
              ...loc.Location.loc_start,
              Lexing.pos_cnum:
                loc.Location.loc_start.Lexing.pos_cnum
                + String.length(s)
                + 1,
            }
        };

        let ast =
          Css_lexer.parse_string(
            ~container_lnum=loc_start.Lexing.pos_lnum,
            ~pos=loc_start,
            str,
            Css_parser.declaration_list,
          );

        createModule(~loc, ~ast);
      }
    | _ => default_mapper.module_expr(mapper, expr)
    },
};

let () = Driver.register(~name="re_styled_ppx", Versions.ocaml_406, moduleMapper);
