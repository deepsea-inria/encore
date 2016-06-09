open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_build = XCmd.mem_flag "virtual_build"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_mode = "replace"   (* later: document the purpose of "mode" *)
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_onlys = XCmd.parse_or_default_list_string "only" []
let arg_sizes = XCmd.parse_or_default_list_string "size" ["all"]
let arg_benchmarks = XCmd.parse_or_default_list_string "benchmark" ["all"]
let arg_proc = XCmd.parse_or_default_list_int "proc" [1; 10; 40]
            
let run_modes =
  Mk_runs.([
    Mode (mode_of_string arg_mode);
    Virtual arg_virtual_run;
    Runs arg_nb_runs; ])


(*****************************************************************************)
(** Steps *)

let select make run check plot =
   let arg_skips =
      if List.mem "run" arg_skips && not (List.mem "make" arg_skips)
         then "make"::arg_skips
         else arg_skips
      in
   Pbench.execute_from_only_skip arg_onlys arg_skips [
      "make", make;
      "run", run;
      "check", check;
      "plot", plot;
      ]

let nothing () = ()

(*****************************************************************************)
(** Files and binaries *)

let build path bs is_virtual =
   system (sprintf "make -C %s -j %s" path (String.concat " " bs)) is_virtual

let file_results exp_name =
  Printf.sprintf "results_%s.txt" exp_name

let file_plots exp_name =
  Printf.sprintf "plots_%s.pdf" exp_name

(** Evaluation functions *)

let eval_exectime = fun env all_results results ->
  Results.get_mean_of "exectime" results

let eval_exectime_stddev = fun env all_results results ->
  Results.get_stddev_of "exectime" results

let formatter_settings = Env.(
    ["prog", Format_custom (fun s -> "")]
  @ ["algorithm", Format_custom (fun s -> s)]
  @ ["n", Format_custom (fun s -> sprintf "Input: %s 32-bit ints" s)]
  @ ["exectime", Format_custom (fun s -> sprintf "Time (s)")]
  @ ["operation", Format_custom (fun s -> s)])

let default_formatter =
 Env.format formatter_settings

(*****************************************************************************)
(** Input data **)
                        
type size = Small | Medium | Large
                               
let string_of_size = function
   | Small -> "small"
   | Medium -> "medium"
   | Large -> "large"

let size_of_string = function
   | "small" -> Small
   | "medium" -> Medium
   | "large" -> Large
   | _ -> Pbench.error "invalid argument for argument size"

let arg_sizes =
   match arg_sizes with
   | ["all"] -> ["small"; "medium"; "large"]
   | _ -> arg_sizes

let sequence_benchmarks = ["comparison_sort"; "blockradix_sort"; "remove_duplicates";
                      "suffix_array"; "convex_hull"; "nearest_neighbours"; "ray_cast"]

let arg_benchmarks = 
   match arg_benchmarks with
   | ["all"] -> sequence_benchmarks
   | ["sequence"] -> sequence_benchmarks
   | _ -> arg_benchmarks

let use_sizes =
   List.map size_of_string arg_sizes

let mk_sizes =
   mk_list string "size" (List.map string_of_size use_sizes)
                       
let mk_generator generator = mk string "generator" generator

let types_list = function
  | "comparison_sort" -> [ "array_double"; "array_string"; ]
  | "blockradix_sort" -> [ "array_int"; "array_pair_int_int"; ]
  | "remove_duplicates" -> [ "array_int"; "array_string"; ] (** "array_pair_string_int"; ]**)
  | "suffix_array" -> [ "string"; ]
  | "convex_hull" -> [ "array_point2d"; ]
  | "nearest_neighbours" -> [ "array_point2d"; "array_point3d"; ]
  | "ray_cast" -> []
  | _ -> Pbench.error "invalid benchmark"

let generators_list = function
  | "comparison_sort" -> (function n ->
    let nb_swaps nb = int_of_float (sqrt (float_of_int nb)) in
    function
    | "array_double" -> [
        mk_generator "random";
        mk_generator "exponential";
        ((mk_generator "almost_sorted") & (mk int "nb_swaps" (nb_swaps n)));
      ]
    | "array_string" -> [
        mk_generator "trigrams";
      ]
    | _ -> Pbench.error "invalid type")
  | "blockradix_sort" -> (function n ->
    function
    | "array_int" -> [
        mk_generator "random";
        mk_generator "exponential";
      ]
    | "array_pair_int_int" -> [
(** TODO solve the problem with the output file name **)
        ((mk_generator "random") & (mk int "m2" 256));
        ((mk_generator "random") & (mk int "m2" n));
      ]
    | _ -> Pbench.error "invalid type")
  | "remove_duplicates" -> (function n ->
    function
    | "array_int" -> [
        mk_generator "random";
        ((mk_generator "random_bounded") & (mk int "m" 100000));
        mk_generator "exponential";
      ]
    | "array_string" -> [
        mk_generator "trigrams";
      ]
(**    | "array_pair_string_int" -> [
        mk_generator "trigrams";
      ]**)
    | _ -> Pbench.error "invalid type")
  | "suffix_array" -> (function n ->
    function
    | "string" -> [
       mk_generator "trigrams";
     ]
    | _ -> Pbench.error "invalid type")
  | "convex_hull" -> (function n ->
    function
    | "array_point2d" -> [
        mk_generator "in_circle";
        mk_generator "kuzmin";
        mk_generator "on_circle";
      ]
    | _ -> Pbench.error "invalid type")
  | "nearest_neighbours" -> (function n ->
    function
    | "array_point2d" -> [
        mk_generator "in_square";
        mk_generator "kuzmin";
      ]
    | "array_point3d" -> [
        mk_generator "in_cube";
        mk_generator "on_sphere";
        mk_generator "plummer";
      ]
    | _ -> Pbench.error "invalid_type")
  | "ray_cast" -> (function n -> function typ -> [])
  | _ -> Pbench.error "invalid benchmark"

let mk_generate_sequence_inputs benchmark : Params.t =
  let load = function
    | Small -> 1000000
    | Medium -> 10000000
    | Large -> 100000000
  in
  let mk_outfile size typ mk_generator =
    mk string "outfile" (sprintf "_data/%s_%s_%s.bin" typ (XList.to_string "_" (fun (k, v) -> sprintf "%s" (Env.string_of_value v)) (Params.to_env mk_generator)) size) in
  let mk_type typ = mk string "type" typ in
  let mk_n n = mk int "n" n in
  let mk_size size = (mk_n (load size)) & (mk string "!size" (string_of_size size)) in
  let use_types = types_list benchmark in
  let mk_generators = generators_list benchmark in
  Params.concat (~~ List.map use_sizes (fun size ->
  Params.concat (~~ List.map use_types (fun typ ->
  let n = load size in
  Params.concat (~~ List.map (mk_generators n typ) (fun mk_generator ->
  (   mk_type typ
    & mk_size size
    & mk_generator
    & (mk_outfile (string_of_size size) typ mk_generator) )))))))

let mk_files_inputs benchmark : Params.t =
  let mk_type typ = mk string "type" typ in
  let mk_infile infile = mk string "infile" infile in
  let mk_infile2 infile2 = mk string "infile2" infile2 in
  let mk_outfile outfile = mk string "outfile" outfile in
  match benchmark with
  | "suffix_array" ->
    (mk_type "string" &
    ((mk_infile "data/chr22.dna.txt" & mk_outfile "_data/chr22.dna.bin") ++
     (mk_infile "data/etext99.txt" & mk_outfile "_data/etext99.bin") ++
     (mk_infile "data/wikisamp.xml.txt" & mk_outfile "_data/wikisamp.xml.bin")))
  | "ray_cast" ->
    (mk_type "ray_cast_test" &
    ((mk_infile "data/happyTriangles.txt" & mk_infile2 "data/happyRays.txt" & mk_outfile "_data/happy_ray_cast_dataset.bin") ++
     (mk_infile "data/angelTriangles.txt" & mk_infile2 "data/angelRays.txt" & mk_outfile "_data/angel_ray_cast_dataset.bin") ++
     (mk_infile "data/dragonTriangles.txt" & mk_infile2 "data/dragonRays.txt" & mk_outfile "_data/dragon_ray_cast_dataset.bin")))
  | _ -> Params.concat ([])

let mk_sequence_inputs benchmark : Params.t =
  (mk_generate_sequence_inputs benchmark) ++ (mk_files_inputs benchmark)

let sequence_descriptor_of mk_sequence_inputs =
   ~~ List.map (Params.to_envs mk_sequence_inputs) (fun e ->
      (Env.get_as_string e "type"),
      (Env.get_as_string e "outfile")
      )

let mk_sequence_input_names benchmark =
  let mk2 = sequence_descriptor_of (mk_sequence_inputs benchmark) in
  Params.eval (Params.concat (~~ List.map mk2 (fun (typ,outfile) ->
    (mk string "type" typ) &
    (mk string "infile" outfile)
  )))            

let mk_lib_types =
  mk_list string "lib_type" [ "pctl"; "pbbs"; ]
              
(*****************************************************************************)
(** Input-data generator *)

module ExpGenerate = struct

let name = "generate"

let prog = "./sequence_data.opt"

let make() =
  build "." [prog] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
      & (Params.concat (~~ List.map arg_benchmarks (fun benchmark ->
         mk_sequence_inputs benchmark)))
    )
  ]))

let check = nothing  (* do something here *)

let plot() = ()

let all () = select make run check plot

end

(*****************************************************************************)
(** Sequence-library benchmark *)

module ExpSequenceLibrary = struct

let name = "sequence"

let prog_encore = name^".encore"

let prog_cilk = name^".cilk"

let mk_encore_setting threshold block_size =
  mk int "threshold" threshold & mk int "block_size" block_size

let mk_encore_settings = (
    mk_encore_setting 1024 2048
 ++ mk_encore_setting 2048 4096
 ++ mk_encore_setting 4098 8192)

let mk_algorithms = (
     (mk_prog prog_encore & mk string "algorithm" "sequential")
  ++ (mk_prog prog_cilk   & mk string "algorithm" "pbbs")
  ++ (  (mk_prog prog_encore & mk string "algorithm" "encore")
      & mk_encore_settings)
)

let mk_operations = mk_list string "operation" ["reduce"; "max_index"; "scan"; "pack"; "filter"; ]

let mk_input_sizes = mk_list int "n" [ 400000000 ]

let make() =
  build "." [prog_encore; prog_cilk] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (mk_algorithms & mk_operations & mk_input_sizes)
  ]))

let check = nothing  (* do something here *)

let plot() =
     Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
(*         Chart_opt Chart.([Dimensions (12.,8.) ]);
         X_titles_dir Vertical;
         Y_axis [ Axis.Lower (Some 0.);
                  Axis.Is_log false ] *)
         ]);
      Formatter default_formatter;
      Charts mk_operations;
      Series mk_algorithms;
      X mk_input_sizes;
      Y_label "exectime";
      Y eval_exectime;
      Y_whiskers eval_exectime_stddev;
      Output (file_plots name);
      Results (Results.from_file (file_results name));
      ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "generate", ExpGenerate.all;
    "sequence", ExpSequenceLibrary.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
