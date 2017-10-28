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
    else if hostname = "aware.aladdin.cs.cmu.edu" then
      [ 72; ]
    else if hostname = "beast" then
      [ 8; ]
    else
      [ 1; ]
  in
  let default =
    if List.exists (fun p -> p = 1) default then
      default
    else
      1 :: default
  in
  XCmd.parse_or_default_list_int "proc" default
let arg_print_err = XCmd.parse_or_default_bool "print_error" false
let arg_scheduler = XCmd.parse_or_default_string "scheduler" "steal_half_work_stealing"
    
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
  @ ["promotion_threshold", Format_custom (fun s -> sprintf "F=%s" s)]
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

let mk_pbbs_algorithm = mk_prog prog_cilk & mk string "algorithm" "pbbs"
                           
let mk_algorithms = (
(*     (mk_prog prog_encore & mk string "algorithm" "sequential")
  ++ *) mk_pbbs_algorithm
(*  ++ (  (mk_prog prog_encore & mk string "algorithm" "encore_sequential")
      & mk_encore_settings) *)
  ++ (  (mk_prog prog_encore & mk string "algorithm" "encore"))
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
    "convexhull"; "samplesort"; "radixsort"; "nearestneighbors";
    "suffixarray"; "removeduplicates"; "mis"; "mst"; "matching";
    "delaunay"; "bfs"; (*"pbfs";*)
    ]
  | _ -> arg_benchmarks
    
let encore_prog_of n = n ^ ".encore"
let cilk_prog_of n = n ^ ".cilk"
let cilk_elision_prog_of n = n ^ ".cilk_elision"
                             
let encore_progs = List.map encore_prog_of all_benchmarks
let cilk_progs = List.map cilk_prog_of all_benchmarks
let cilk_elision_progs = List.map cilk_elision_prog_of all_benchmarks
let all_progs = List.concat [encore_progs; cilk_progs; cilk_elision_progs]

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
    
let input_descriptor_hull = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_circle_large.bin", string "2d", "in circle";
  "array_point2d_kuzmin_large.bin", string "2d", "kuzmin";
  "array_point2d_on_circle_medium.bin", string  "2d", "on circle";
]

let mk_hull_infiles = mk_infiles "type" input_descriptor_hull
                                 
let mk_encore_prog n =
    (mk string "prog" (encore_prog_of n))
  & (mk string "algorithm" "encore")
(*  & (mk string "scheduler" arg_scheduler)        *)
(*  & (mk int "sharing_threshold" 0)*)

let mk_pbbs_lib =
  mk string "algorithm" "pbbs"
    
let mk_pbbs_prog n =
    (mk string "prog" (cilk_prog_of n))
  & mk_pbbs_lib

let mk_pbbs_elision_prog n =
    (mk string "prog" (cilk_elision_prog_of n))
  & mk_pbbs_lib
    
type input_descriptor =
    string * Env.value * string (* file name, type, pretty name *)
    
type benchmark_descriptor = {
  bd_name : string;
  bd_infiles : Params.t;
  bd_input_descr : input_descriptor list;
}

(*****************)
(* Sample sort *)
  
let input_descriptor_samplesort = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_double_random_large.bin", string "double", "random";
  "array_double_exponential_large.bin", string "double", "exponential";
  "array_double_almost_sorted_10000_large.bin", string "double", "almost sorted";
]
    
let mk_samplesort_infiles = mk_infiles "type" input_descriptor_samplesort

(*****************)
(* Radix sort *)

let input_descriptor_radixsort = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_int_random_large.bin", string "int", "random";    
  "array_int_exponential_large.bin", string "int", "exponential";
  "array_pair_int_int_random_256_large.bin", string "pair_int_int", "random int pair 256";
  "array_pair_int_int_random_100000000_large.bin", string "pair_int_int", "random int pair 10m";
]

let mk_radixsort_infiles = mk_infiles "type" input_descriptor_radixsort

(*****************)
(* BFS *)
      
let input_descriptor_bfs = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "cube_large.bin", int 0, "cube";
  "rmat24_large.bin", int 0, "rMat24";
  "rmat27_large.bin", int 0, "rMat27";
]

let mk_bfs_infiles = mk_infiles "source" input_descriptor_bfs
    
(*****************)
(* PBFS *)

let input_descriptor_pbfs = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "cube_large.bin", int 0, "cube";
  "rmat24_large.bin", int 0, "rMat24";
  "rmat27_large.bin", int 0, "rMat27";
]

let mk_pbfs_infiles = mk_infiles "source" input_descriptor_pbfs

(*****************)
(* MIS *)

let input_descriptor_mis = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "cube_large.bin", int 0, "cube";
  "rmat24_large.bin", int 0, "rMat24";
  "rmat27_large.bin", int 0, "rMat27";
]

let mk_mis_infiles = mk_infiles "source" input_descriptor_mis

(*****************)
(* MST *)

let input_descriptor_mst = input_descriptor_mis

let mk_mst_infiles = mk_infiles "source" input_descriptor_mst

(*****************)
(* Matching *)

let input_descriptor_matching = input_descriptor_mis

let mk_matching_infiles = mk_infiles "source" input_descriptor_matching

(*****************)
(* Suffix array *)

let input_descriptor_suffixarray = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "chr22.dna.bin", string "string", "dna";
  "etext99.bin", string "string", "etext";
  "wikisamp.xml.bin", string "string", "wikisamp";
]
      
let mk_suffixarray_infiles = mk_infiles "type" input_descriptor_suffixarray

(*****************)
(* Nearest neighbors *)

let input_descriptor_nearestneighbors = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_kuzmin_medium.bin", string "array_point2d", "kuzmin";
  "array_point3d_on_sphere_medium.bin", string "array_point3d", "on sphere";
  "array_point3d_plummer_medium.bin", string "array_point3d", "plummer";
  (*  "array_point2d_in_square_medium.bin", string "array_point2d", "in square";*)
(*  "array_point3d_in_cube_medium.bin", string "array_point3d", "in cube"; *)
]

let mk_nearestneighbors_infiles = mk_infiles "type" input_descriptor_nearestneighbors

(*****************)
(* Delaunay *)

let input_descriptor_delaunay = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_point2d_in_square_delaunay_large.bin", string "array_point2d", "in square";
  "array_point2d_kuzmin_delaunay_large.bin", string "array_point2d", "kuzmin";
]

let mk_delaunay_infiles = mk_infiles "type" input_descriptor_delaunay

(*****************)
(* Remove duplicates *)

let input_descriptor_removeduplicates = List.map (fun (p, t, n) -> (path_to_infile p, t, n)) [
  "array_int_random_large.bin", string "array_int", "random";
  "array_int_random_bounded_100000_large.bin", string "array_int", "bounded random";
  "array_int_exponential_large.bin", string "array_int", "exponential";
  "array_string_trigrams_large.bin", string "array_string", "string trigrams";
]

let mk_removeduplicates_infiles = mk_infiles "type" input_descriptor_removeduplicates

(*****************)
(* All benchmarks *)

let benchmarks' : benchmark_descriptor list = [
  { bd_name = "convexhull";
    bd_infiles = mk_hull_infiles;
    bd_input_descr = input_descriptor_hull;
  };
  { bd_name = "samplesort";
    bd_infiles = mk_samplesort_infiles;
    bd_input_descr = input_descriptor_samplesort;
  };
  { bd_name = "radixsort";
    bd_infiles = mk_radixsort_infiles;
    bd_input_descr = input_descriptor_radixsort;
  };
  { bd_name = "bfs";
    bd_infiles = mk_bfs_infiles;
    bd_input_descr = input_descriptor_bfs;
  }; 
  { bd_name = "pbfs";
    bd_infiles = mk_pbfs_infiles;
    bd_input_descr = input_descriptor_pbfs;
  }; 
  { bd_name = "mis";
    bd_infiles = mk_mis_infiles;
    bd_input_descr = input_descriptor_mis;
  }; 
  { bd_name = "mst";
    bd_infiles = mk_mst_infiles;
    bd_input_descr = input_descriptor_mst;
  }; 
  { bd_name = "matching";
    bd_infiles = mk_matching_infiles;
    bd_input_descr = input_descriptor_matching;
  }; 
  { bd_name = "suffixarray";
    bd_infiles = mk_suffixarray_infiles;
    bd_input_descr = input_descriptor_suffixarray;
  };
  { bd_name = "nearestneighbors";
    bd_infiles = mk_nearestneighbors_infiles;
    bd_input_descr = input_descriptor_nearestneighbors;
  };
  { bd_name = "delaunay";
    bd_infiles = mk_delaunay_infiles;
    bd_input_descr = input_descriptor_delaunay;
  };
  { bd_name = "removeduplicates";
    bd_infiles = mk_removeduplicates_infiles;
    bd_input_descr = input_descriptor_removeduplicates;
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
  | (m, _, p) :: _ -> p
  | [] -> failwith ("pretty name: " ^ n)
                  
let make() =
  build "." all_progs arg_virtual_build

let mk_never_promote =
  mk int "never_promote" 1

let file_results_never_promote exp_name =
  file_results (exp_name ^ "_never_promote")

let file_results_pbbs_elision exp_name =
  file_results (exp_name ^ "_cilk_elision")
        
let run() =
  List.iter (fun benchmark ->
    let r mk_progs file_results = 
      Mk_runs.(call (run_modes @ [
        Output file_results;
        Timeout 400;
        Args (mk_progs & benchmark.bd_infiles); ]))
    in
    let encore_prog = mk_encore_prog benchmark.bd_name in
    let pbbs_prog = mk_pbbs_prog benchmark.bd_name in
    let pbbs_elision_prog = mk_pbbs_elision_prog benchmark.bd_name in
    (r ((encore_prog ++ pbbs_prog) & mk_proc) (file_results benchmark.bd_name);
     if List.exists (fun p -> p = 1) arg_proc then (
       r ((encore_prog & mk_never_promote) & mk int "proc" 1) (file_results_never_promote benchmark.bd_name);
       r (pbbs_elision_prog & mk int "proc" 1) (file_results_pbbs_elision benchmark.bd_name);
      )
     else
       ())
  ) benchmarks

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
    let nb_columns = nb_proc * 2 + 2 in
    Mk_table.build_table tex_file pdf_file (fun add ->
      let hdr =
        let ls = String.concat "|" (XList.init nb_columns (fun _ -> "c")) in
        Printf.sprintf "p{1cm}l|%s" ls
      in
      add (Latex.tabular_begin hdr);
      Mk_table.cell ~escape:true ~last:false add (Latex.tabular_multicol 2 "l|" "Application/input");
      ~~ List.iteri arg_proc (fun i proc ->
        let last = i + 1 = nb_proc in
        let label = Printf.sprintf "Nb. Cores %d" proc in
        let str = if last then "c" else "c|" in
        let nbcol = if proc = 1 then 4 else 2 in
        let label = Latex.tabular_multicol nbcol str label in
        Mk_table.cell ~escape:false ~last:last add label);
      add Latex.tabular_newline;

      let _ = Mk_table.cell ~escape:false ~last:false add "" in
      let _ = Mk_table.cell ~escape:false ~last:false add "" in
      ~~ List.iteri arg_proc (fun i proc ->
        let last = i + 1 = nb_proc in
        let pbbs_str = "PBBS" in
        let encore_str = "Encore" in
        (if proc = 1 then (
          Mk_table.cell add (Latex.tabular_multicol 2 "c|" pbbs_str);
          Mk_table.cell add (Latex.tabular_multicol 2 "c|" encore_str))
        else (
          Mk_table.cell ~escape:true ~last:false add pbbs_str;
          Mk_table.cell ~escape:true ~last:last add encore_str)));
      add Latex.tabular_newline;

      Mk_table.cell add (Latex.tabular_multicol 2 "l|" "");
      ~~ List.iteri arg_proc (fun i proc ->
        let last = i + 1 = nb_proc in
        if proc = 1 then (
          Mk_table.cell ~escape:true ~last:false add "Time (s)";
          Mk_table.cell ~escape:true ~last:false add "\\begin{tabular}[x]{@{}c@{}}Rel.\\\\elision\\end{tabular}";
          Mk_table.cell ~escape:true ~last:false add "\\begin{tabular}[x]{@{}c@{}}Rel.\\\\PBBS\\end{tabular}";
          Mk_table.cell ~escape:true ~last:last add "\\begin{tabular}[x]{@{}c@{}}Rel.\\\\elision\\end{tabular}")
        else (
          Mk_table.cell ~escape:true ~last:false add "";
          Mk_table.cell ~escape:true ~last:last add ""));
      add Latex.tabular_newline;

      ~~ List.iteri benchmarks (fun benchmark_i benchmark ->
        Mk_table.cell add (Latex.tabular_multicol 2 "l|" (sprintf "\\textbf{%s}" (Latex.escape benchmark.bd_name)));
        for i = 1 to nb_columns - 2 do
          let last = i = nb_columns in
          Mk_table.cell ~escape:true ~last:last add "";
        done;
        add Latex.tabular_newline;
        let results_file = file_results benchmark.bd_name in
        let all_results = Results.from_file results_file in
        let results = all_results in
        let env = Env.empty in
        let mk_rows = benchmark.bd_infiles in
        let env_rows = mk_rows env in
        let results_file_never_promote = file_results_never_promote benchmark.bd_name in
        let results_never_promote = Results.from_file results_file_never_promote in
        let results_file_pbbs_elision = file_results_pbbs_elision benchmark.bd_name in
        let results_pbbs_elision = Results.from_file results_file_pbbs_elision in
        ~~ List.iter env_rows (fun env_rows ->  (* loop over each input for current benchmark *)
          let results = Results.filter env_rows results in
          let results_never_promote = Results.filter env_rows results_never_promote in
          let results_pbbs_elision = Results.filter env_rows results_pbbs_elision in          
          let env = Env.append env env_rows in
          let row_title = main_formatter env_rows in
          let _ = Mk_table.cell ~escape:true ~last:false add "" in
          let _ = Mk_table.cell ~escape:true ~last:false add row_title in
          ~~ List.iteri arg_proc (fun proc_i proc ->
            let last = proc_i + 1 = nb_proc in
            let mk_procs = mk int "proc" proc in
            let (pbbs_str, b, elision_str) =
              let [col_elision] = ((mk_pbbs_elision_prog benchmark.bd_name) & mk_procs) env in
              let env_elision = Env.append env col_elision in
              let [col] = ((mk_pbbs_prog benchmark.bd_name) & mk_procs) env in
              let env = Env.append env col in
              let results = Results.filter col results in
              let v = eval_exectime env all_results results in
              let e = eval_exectime_stddev env all_results results in
              let err =  if arg_print_err then Printf.sprintf "(%.2f%s)"  e "$\\sigma$" else "" in
              let elision_str =
                if proc = 1 then
                  let results = Results.filter col_elision results_pbbs_elision in
                  let v' = Results.get_mean_of "exectime" results_pbbs_elision in
                  let vs = string_of_percentage_change v v' in
                  vs
                else
                  ""
              in
              (Printf.sprintf "%.3f %s" v err, v, elision_str)
            in
            (if proc = 1 then (
              Mk_table.cell ~escape:false ~last:false add pbbs_str;
              Mk_table.cell ~escape:false ~last:false add elision_str)
            else (
              Mk_table.cell ~escape:false ~last:false add pbbs_str)); 
            let (encore_str, never_promote_str) = 
              let [col] = ((mk_encore_prog benchmark.bd_name) & mk_procs) env in
              let results = Results.filter col results in
              let v = Results.get_mean_of "exectime" results in
              let encore_str = string_of_percentage_change b v in
              let never_promote_str =
                if proc = 1 then
                  let [col] = (mk_encore_prog benchmark.bd_name & mk_never_promote & (mk int "proc" 1)) env in
                  let results = Results.filter col results_never_promote in
                  let v' = Results.get_mean_of "exectime" results_never_promote in
                  let vs = string_of_percentage_change v v' in
                  vs
                else
                  ""
              in
              (encore_str, never_promote_str)
            in
            if proc = 1 then (
              Mk_table.cell ~escape:false ~last:false add encore_str;
              Mk_table.cell ~escape:false ~last:last add never_promote_str)
            else
              Mk_table.cell ~escape:false ~last:last add encore_str);
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
