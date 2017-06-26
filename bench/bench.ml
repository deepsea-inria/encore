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
let arg_proc = XCmd.parse_or_default_int "proc" 40
            
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
    ]
  | _ -> arg_benchmarks
    
let encore_prog_of n = n ^ ".encore"
let cilk_prog_of n = n ^ ".cilk"

let encore_progs = List.map encore_prog_of all_benchmarks
let cilk_progs = List.map cilk_prog_of all_benchmarks
let all_progs = List.concat [encore_progs; cilk_progs]

let mk_proc = mk int "proc" arg_proc

let path_to_infile n = "_data/" ^ n

let mk_infiles ty descr = fun e ->
  let f (p, t, n) =
    let e0 = 
      Env.add Env.empty "infile" (string p)
    in
    Env.add e0 ty (string t)
  in
  List.map f descr

(*****************)
(* Convex hull *)

let prog_hull =
  "convexhull"
    
let input_descriptor_hull = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_circle_large.bin", "2d", "in circle";
  "array_point2d_kuzmin_large.bin", "2d", "kuzmin";
  "array_point2d_on_circle_medium.bin", "2d", "on circle";
]

let mk_hull_infiles = mk_infiles "type" input_descriptor_hull

let mk_progs n =
  ((mk string "prog" (encore_prog_of n)) & (mk string "algorithm" "encore")) ++
  ((mk string "prog" (cilk_prog_of n)) & (mk string "algorithm" "pbbs"))

let mk_hull_progs =
  mk_progs prog_hull

let mk_convexhull =
    mk_hull_progs
  & mk_proc
  & mk_hull_infiles

type benchmark_descriptor = {
  bd_name : string;
  bd_args : Params.t;
  bd_infiles : Params.t;
  bd_progs : Params.t;    
}

(*****************)
(* Sample sort *)

let prog_samplesort =
  "samplesort"

let mk_samplesort_progs =
  mk_progs prog_samplesort  
  
let input_descriptor_samplesort = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_double_random_large.bin", "double", "random";
  "array_double_exponential_large.bin", "double", "exponential";
  "array_double_almost_sorted_10000_large.bin", "double", "almost sorted";
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
  "array_int_random_large.bin", "int", "random";    
  "array_int_exponential_large.bin", "int", "exponential";
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
  "cube_large.bin", "0", "cube";
  "wikipedia-20070206.bin", "0", "wikipedia";
  "rmat24_large.bin", "0", "rMat24";
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
(* Nearest neighbors *)

let prog_nn =
  "nearestneighbors"

let mk_nn_progs =
  mk_progs prog_nn

let input_descriptor_nn = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_square_large.bin", "array_point2d", "in square";
  "array_point2d_kuzmin_large.bin", "array_point2d", "kuzmin";
  "array_point3d_in_cube_large.bin", "array_point3d", "in cube";
  "array_point3d_on_sphere_large.bin", "array_point3d", "on sphere";
  "array_point3d_plummer_large.bin", "array_point3d", "plummer"; 
]

let mk_nn_infiles = mk_infiles "type" input_descriptor_nn

let mk_nn =
    mk_nn_progs
  & mk_proc
  & mk_nn_infiles

(*****************)
(* All benchmarks *)

let benchmarks' : benchmark_descriptor list = [
  { bd_name = "convexhull"; bd_args = mk_convexhull;
    bd_infiles = mk_hull_infiles; bd_progs = mk_hull_progs;
  };
  { bd_name = "samplesort"; bd_args = mk_samplesort;
    bd_infiles = mk_samplesort_infiles; bd_progs = mk_samplesort_progs;
  };
  { bd_name = "radixsort"; bd_args = mk_radixsort;
    bd_infiles = mk_radixsort_infiles; bd_progs = mk_radixsort_progs;
  };
  { bd_name = "pbfs"; bd_args = mk_pbfs;
    bd_infiles = mk_pbfs_infiles; bd_progs = mk_pbfs_progs;
  };
  { bd_name = "mis"; bd_args = mk_mis;
    bd_infiles = mk_pbfs_infiles; bd_progs = mk_mis_progs;
  };
  { bd_name = "nearestneighbors"; bd_args = mk_nn;
    bd_infiles = mk_pbfs_infiles; bd_progs = mk_nn_progs;
  };
]

let benchmarks =
  let p b =
    List.exists (fun a -> b.bd_name = a) all_benchmarks
  in
  List.filter p benchmarks'

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
  List.iter (fun benchmark ->
     Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
                              (* Chart_opt Chart.([Dimensions (12.,8.) ]);*)
                              X_titles_dir Vertical;
         Y_axis [ Axis.Lower (Some 0.); Axis.Upper (Some 2.5);
                  Axis.Is_log false ] 
         ]);
      Formatter default_formatter;
      Charts mk_unit;
      Series benchmark.bd_infiles;
      X benchmark.bd_progs;
      Y_label "Time (s)";
      Y eval_exectime;
      Y_whiskers eval_exectime_stddev;
      Output (file_plots benchmark.bd_name);
      Results (Results.from_file (file_results benchmark.bd_name));
      ]))) benchmarks

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
