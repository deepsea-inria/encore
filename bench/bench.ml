open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_build = XCmd.mem_flag "virtual_build"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_mode = Mk_runs.mode_from_command_line "mode"
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_onlys = XCmd.parse_or_default_list_string "only" []
let arg_benchmarks = XCmd.parse_or_default_list_string "benchmark" ["all"]
let arg_proc =
  let hostname = Unix.gethostname () in
  let default =
    if hostname = "teraram" then
      [ 40; ]
    else if hostname = "cadmium" then
      [ 48; ]
    else if hostname = "hiphi.aladdin.cs.cmu.edu" then
      [ 64; ]
    else
      [ 1; ]
  in
  XCmd.parse_or_default_list_int "proc" default
let arg_print_err = XCmd.parse_or_default_bool "print_error" false
    
let run_modes =
  Mk_runs.([
    Mode arg_mode;
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

let file_tables_src exp_name =
  Printf.sprintf "tables_%s.tex" exp_name

let file_tables exp_name =
  Printf.sprintf "tables_%s.pdf" exp_name

let file_plots exp_name =
  Printf.sprintf "plots_%s.pdf" exp_name

(** Evaluation functions *)

let eval_exectime = fun env all_results results ->
  Results.get_mean_of "exectime" results

let eval_exectime_stddev = fun env all_results results ->
  Results.get_stddev_of "exectime" results

let string_of_millions ?(munit=false) v =
   let x = v /. 1000000. in
   let f = 
     if x >= 10. then sprintf "%.0f" x
     else if x >= 1. then sprintf "%.1f" x
     else if x >= 0.1 then sprintf "%.2f" x
     else sprintf "%.3f" x in
   f ^ (if munit then "m" else "")
                        
let formatter_settings = Env.(
    ["prog", Format_custom (fun s -> "")]
  @ ["algorithm", Format_custom (fun s -> s)]
  @ ["n", Format_custom (fun s -> sprintf "Input: %s million 32-bit ints" (string_of_millions (float_of_string s)))]
  @ ["proc", Format_custom (fun s -> sprintf "#CPUs %s" s)]
  @ ["dag_freq", Format_custom (fun s -> sprintf "F=%s" s)]
  @ ["threshold", Format_custom (fun s -> sprintf "K=%s" s)]
  @ ["block_size", Format_custom (fun s -> sprintf "B=%s" s)]      
  @ ["operation", Format_custom (fun s -> s)])

let default_formatter =
  Env.format formatter_settings
    
let string_of_percentage_value v =
  let x = 100. *. v in
  (* let sx = if abs_float x < 10. then (sprintf "%.1f" x) else (sprintf "%.0f" x)  in *)
  let sx = sprintf "%.1f" x in
  sx
    
let string_of_percentage ?(show_plus=true) v =
   match classify_float v with
   | FP_subnormal | FP_zero | FP_normal ->
       sprintf "%s%s%s"  (if v > 0. && show_plus then "+" else "") (string_of_percentage_value v) "\\%"
   | FP_infinite -> "$+\\infty$"
   | FP_nan -> "na"

let string_of_percentage_change ?(show_plus=true) vold vnew =
  string_of_percentage ~show_plus:show_plus (vnew /. vold -. 1.0)

(*****************************************************************************)
(** Sequence-library benchmark *)

module ExpSequenceLibrary = struct

let name = "sequence"

let prog_encore = name^".encore"

let prog_cilk = name^".cilk"

let mk_encore_setting threshold block_size dag_freq =
    mk int "threshold" threshold
  & mk int "block_size" block_size
  & mk int "dag_freq" dag_freq

let mk_encore_settings = (
    mk_encore_setting 512  1024 1024
 ++ mk_encore_setting 1024 2048 1024
 ++ mk_encore_setting 2048 4096 1024)

let mk_pbbs_algorithm = mk_prog prog_cilk & mk string "algorithm" "pbbs"
                           
let mk_algorithms = (
(*     (mk_prog prog_encore & mk string "algorithm" "sequential")
  ++ *) mk_pbbs_algorithm
(*  ++ (  (mk_prog prog_encore & mk string "algorithm" "encore_sequential")
      & mk_encore_settings) *)
  ++ (  (mk_prog prog_encore & mk string "algorithm" "encore")
      & mk_encore_settings)
)

let mk_operations = mk_list string "operation" [ "reduce"; "max_index"; "scan"; "pack"; "filter"; ] 

let mk_input_sizes = mk_list int "n" [ 400000000 ]

let mk_procs = mk_list int "proc" [ 1; (* 10; 20; 30; *) 40; ]

let make() =
  build "." [prog_encore; prog_cilk] arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (mk_algorithms & mk_procs & mk_operations & mk_input_sizes)
  ]))

let check = nothing  (* do something here *)

let plot() =
     Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
                              (* Chart_opt Chart.([Dimensions (12.,8.) ]);*)
                              X_titles_dir Vertical;
         Y_axis [ Axis.Lower (Some 0.); Axis.Upper (Some 2.5);
                  Axis.Is_log false ] 
         ]);
      Formatter default_formatter;
      Charts (mk_procs & mk_input_sizes);
      Series mk_algorithms;
      X mk_operations;
      Y_label "Time (s)";
      Y eval_exectime;
      Y_whiskers eval_exectime_stddev;
      Output (file_plots name);
      Results (Results.from_file (file_results name));
      ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Comparison benchmark *)

module ExpCompare = struct

let name = "compare"

let all_benchmarks =
  match arg_benchmarks with
  | ["all"] -> [
    "convexhull"; "samplesort"; "radixsort"; "pbfs"; "mis"; "nearestneighbors";
    "suffixarray";
    ]
  | _ -> arg_benchmarks
    
let encore_prog_of n = n ^ ".encore"
let cilk_prog_of n = n ^ ".cilk"

let encore_progs = List.map encore_prog_of all_benchmarks
let cilk_progs = List.map cilk_prog_of all_benchmarks
let all_progs = List.concat [encore_progs; cilk_progs]

let mk_proc = mk_list int "proc" arg_proc

let path_to_infile n = "_data/" ^ n

let mk_infiles ty descr = fun e ->
  let f (p, t, n) =
    let e0 = 
      Env.add Env.empty "infile" (string p)
    in
    Env.add e0 ty t
  in
  List.map f descr

(*****************)
(* Convex hull *)

let prog_hull =
  "convexhull"
    
let input_descriptor_hull = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_circle_large.bin", string "2d", "in circle";
  "array_point2d_kuzmin_large.bin", string "2d", "kuzmin";
  "array_point2d_on_circle_medium.bin", string  "2d", "on circle";
]

let mk_hull_infiles = mk_infiles "type" input_descriptor_hull

let mk_encore_prog n =
  (mk string "prog" (encore_prog_of n)) & (mk string "algorithm" "encore")

let mk_pbbs_prog n =
  (mk string "prog" (cilk_prog_of n)) & (mk string "algorithm" "pbbs")
    
let mk_progs n =
  (mk_encore_prog n) ++ (mk_pbbs_prog n)

let mk_hull_progs =
  mk_progs prog_hull

let mk_convexhull =
    mk_hull_progs
  & mk_proc
  & mk_hull_infiles

type input_descriptor =
    string * Env.value * string (* file name, type, pretty name *)
    
type benchmark_descriptor = {
  bd_name : string;
  bd_args : Params.t;
  bd_infiles : Params.t;
  bd_progs : Params.t;
  bd_input_descr : input_descriptor list;
}

(*****************)
(* Sample sort *)

let prog_samplesort =
  "samplesort"

let mk_samplesort_progs =
  mk_progs prog_samplesort  
  
let input_descriptor_samplesort = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_double_random_large.bin", string "double", "random";
  "array_double_exponential_large.bin", string "double", "exponential";
  "array_double_almost_sorted_10000_large.bin", string "double", "almost sorted";
]
    
let mk_samplesort_infiles = mk_infiles "type" input_descriptor_samplesort

let mk_samplesort =
    mk_samplesort_progs
  & mk_proc
  & mk_samplesort_infiles

(*****************)
(* Radix sort *)

let prog_radixsort =
  "radixsort"

let mk_radixsort_progs =
  mk_progs prog_radixsort

let input_descriptor_radixsort = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_int_random_large.bin", string "int", "random";    
  "array_int_exponential_large.bin", string "int", "exponential";
]

let mk_radixsort_infiles = mk_infiles "type" input_descriptor_radixsort
    
let mk_radixsort =
    mk_radixsort_progs
  & mk_proc
  & mk_radixsort_infiles

(*****************)
(* BFS *)

let prog_pbfs =
  "pbfs"

let mk_pbfs_progs =
  mk_progs prog_pbfs

let input_descriptor_pbfs = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "cube_large.bin", int 0, "cube";
  "wikipedia-20070206.bin", int 0, "wikipedia";
  "rmat24_large.bin", int 0, "rMat24";
]

let mk_pbfs_infiles = mk_infiles "source" input_descriptor_pbfs

let mk_pbfs =
    mk_pbfs_progs
  & mk_proc
  & mk_pbfs_infiles

(*****************)
(* Maximum Independent Set *)

let prog_mis =
  "mis"

let mk_mis_progs =
  mk_progs prog_mis

let mk_mis =
    mk_mis_progs
  & mk_proc
  & mk_pbfs_infiles

(*****************)
(* Suffix array *)

let prog_suffixarray =
  "suffixarray"

let mk_suffixarray_progs =
  mk_progs prog_suffixarray

let input_descriptor_suffixarray = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "chr22.dna.bin", string "string", "dna";
  "etext99.bin", string "string", "etext";
  "wikisamp.xml.bin", string "string", "wikisamp";
]
      
let mk_suffixarray_infiles = mk_infiles "type" input_descriptor_suffixarray

let mk_suffixarray =
    mk_suffixarray_progs
  & mk_proc
  & mk_suffixarray_infiles

(*****************)
(* Nearest neighbors *)

let prog_nearestneighbors =
  "nearestneighbors"

let mk_nearestneighbors_progs =
  mk_progs prog_nearestneighbors

let input_descriptor_nearestneighbors = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_square_large.bin", string "array_point2d", "in square";
  "array_point2d_kuzmin_large.bin", string "array_point2d", "kuzmin";
  "array_point3d_in_cube_large.bin", string "array_point3d", "in cube";
  "array_point3d_on_sphere_large.bin", string "array_point3d", "on sphere";
  "array_point3d_plummer_large.bin", string "array_point3d", "plummer"; 
]

let mk_nearestneighbors_infiles = mk_infiles "type" input_descriptor_nearestneighbors

let mk_nearestneighbors =
    mk_nearestneighbors_progs
  & mk_proc
  & mk_nearestneighbors_infiles

(*****************)
(* All benchmarks *)

let benchmarks' : benchmark_descriptor list = [
  { bd_name = "convexhull"; bd_args = mk_convexhull;
    bd_infiles = mk_hull_infiles; bd_progs = mk_hull_progs;
    bd_input_descr = input_descriptor_hull;
  };
  { bd_name = "samplesort"; bd_args = mk_samplesort;
    bd_infiles = mk_samplesort_infiles; bd_progs = mk_samplesort_progs;
    bd_input_descr = input_descriptor_samplesort;
  };
  { bd_name = "radixsort"; bd_args = mk_radixsort;
    bd_infiles = mk_radixsort_infiles; bd_progs = mk_radixsort_progs;
    bd_input_descr = input_descriptor_radixsort;
  };
  { bd_name = "pbfs"; bd_args = mk_pbfs;
    bd_infiles = mk_pbfs_infiles; bd_progs = mk_pbfs_progs;
    bd_input_descr = input_descriptor_pbfs;
  };
  { bd_name = "mis"; bd_args = mk_mis;
    bd_infiles = mk_pbfs_infiles; bd_progs = mk_mis_progs;
    bd_input_descr = input_descriptor_pbfs;
  };
  { bd_name = "nearestneighbors"; bd_args = mk_nearestneighbors;
    bd_infiles = mk_nearestneighbors_infiles; bd_progs = mk_nearestneighbors_progs;
    bd_input_descr = input_descriptor_nearestneighbors;
  };
  { bd_name = "suffixarray"; bd_args = mk_suffixarray;
    bd_infiles = mk_suffixarray_infiles; bd_progs = mk_suffixarray_progs;
    bd_input_descr = input_descriptor_suffixarray;
  };
]

let benchmarks =
  let p b =
    List.exists (fun a -> b.bd_name = a) all_benchmarks
  in
  List.filter p benchmarks'

let input_descriptors =
  List.flatten (List.map (fun b -> b.bd_input_descr) benchmarks)

let pretty_input_name n =
  match List.find_all (fun (m, _, _) -> m = n) input_descriptors with
  | [(m, _, p)] -> p
  | _ -> failwith "pretty name"
    
let make() =
  build "." all_progs arg_virtual_build

let run() =
  List.iter (fun benchmark ->
    Mk_runs.(call (run_modes @ [
      Output (file_results benchmark.bd_name);
      Timeout 400;
      Args benchmark.bd_args;
    ]))) benchmarks

let check = nothing  (* do something here *)

let plot() =
    let tex_file = file_tables_src name in
    let pdf_file = file_tables name in
    let nb_proc = List.length arg_proc in
    let main_formatter =
      Env.format (Env.(
                  [
                   ("proc", Format_custom (fun n -> ""));
                   ("lib_type", Format_custom (fun n -> ""));
                   ("infile", Format_custom pretty_input_name);
                   ("prog", Format_custom (fun n -> ""));
                   ("type", Format_custom (fun n -> ""));
                   ("source", Format_custom (fun n -> ""));
                 ]
                 ))
    in
    Mk_table.build_table tex_file pdf_file (fun add ->
      let hdr =
        let m = nb_proc * 2 in
        let ls = String.concat "|" (XList.init m (fun _ -> "l")) in
        Printf.sprintf "p{1cm}l|%s" ls
      in
      add (Latex.tabular_begin hdr);
      Mk_table.cell ~escape:true ~last:false add (Latex.tabular_multicol 2 "l|" "Application/input");
      ~~ List.iteri arg_proc (fun i proc ->
        let last = i + 1 = nb_proc in
        let label = Printf.sprintf "Nb. Cores %d" proc in
        let str = if last then "c" else "c|" in
        let label = Latex.tabular_multicol 2 str label in
        Mk_table.cell ~escape:false ~last:last add label);
      add Latex.tabular_newline;

      let _ = Mk_table.cell ~escape:false ~last:false add "" in
      let _ = Mk_table.cell ~escape:false ~last:false add "" in
      ~~ List.iteri arg_proc (fun i proc ->
        let last = i + 1 = nb_proc in
        let pbbs_str = "\\begin{tabular}[x]{@{}c@{}}Time (s)\\\\PBBS\\end{tabular}" in
        let encore_str = "\\begin{tabular}[x]{@{}c@{}}Time (s)\\\\Encore\\end{tabular}" in
        Mk_table.cell ~escape:true ~last:false add pbbs_str;
        Mk_table.cell ~escape:true ~last:last add encore_str);
      add Latex.tabular_newline;

      ~~ List.iteri benchmarks (fun benchmark_i benchmark ->
        Mk_table.cell add (Latex.tabular_multicol 2 "l|" (sprintf "\\textbf{%s}" (Latex.escape benchmark.bd_name)));
        add Latex.tabular_newline;
        let results_file = file_results benchmark.bd_name in
        let all_results = Results.from_file results_file in
        let results = all_results in
        let env = Env.empty in
        let mk_rows = benchmark.bd_infiles in
        let env_rows = mk_rows env in
        ~~ List.iter env_rows (fun env_rows ->  (* loop over each input for current benchmark *)
          let results = Results.filter env_rows results in
          let env = Env.append env env_rows in
          let row_title = main_formatter env_rows in
          let _ = Mk_table.cell ~escape:true ~last:false add "" in
          let _ = Mk_table.cell ~escape:true ~last:false add row_title in
          ~~ List.iteri arg_proc (fun proc_i proc ->
            let last = proc_i + 1 = nb_proc in
            let mk_procs = mk int "proc" proc in
            let (pbbs_str, b) = 
              let [col] = ((mk_pbbs_prog benchmark.bd_name) & mk_procs) env in
              let env = Env.append env col in
              let results = Results.filter col results in
              let v = eval_exectime env all_results results in
              let e = eval_exectime_stddev env all_results results in
              let err =  if arg_print_err then Printf.sprintf "(%.2f%s)"  e "$\\sigma$" else "" in
              (Printf.sprintf "%.3f %s" v err, v)
            in
            Mk_table.cell ~escape:false ~last:false add pbbs_str;
            let pctl_str = 
              let [col] = ((mk_encore_prog benchmark.bd_name) & mk_procs) env in
              let results = Results.filter col results in
              let v = Results.get_mean_of "exectime" results in
              let vs = string_of_percentage_change b v in
              Printf.sprintf "%s" vs
            in
            Mk_table.cell ~escape:false ~last:last add pctl_str);
          add Latex.tabular_newline);            
      );
      add Latex.tabular_end;
      add Latex.new_page;
      ())

let all () = select make run check plot

end
    
(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "sequence", ExpSequenceLibrary.all;
    "compare", ExpCompare.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
