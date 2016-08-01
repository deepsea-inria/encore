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
let arg_max_proc = XCmd.parse_or_default_int "max_proc" 40

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

let eval_nb_operations_per_second = fun env all_results results ->
  let t = eval_exectime env all_results results in
  let nb_operations = Results.get_mean_of "n" results in
  let nb_proc = Env.get_as_float env "proc" in
  let nb_operations_per_proc = nb_operations /. nb_proc in
  nb_operations_per_proc /. t
                     
let eval_nb_operations_per_second_error = fun env all_results results ->
  let ts = Results.get Env.as_float "exectime" results in
  let nb_operations = Results.get Env.as_float "n" results in
  let nb_proc = Env.get_as_float env "proc" in
  let nb_operations_per_proc = List.map (fun nb_operations -> nb_operations /. nb_proc) nb_operations in  
  let ps = List.map (fun (x, y) -> x /. y) (List.combine nb_operations_per_proc ts) in
  let (_, stddev) = XFloat.list_mean_and_stddev ps in
  stddev

let nb_proc = arg_max_proc

let mk_proc = mk int "proc" nb_proc

let mk_algo_dyn = mk string "algo" "dyn"

let mk_algo_sim = mk string "algo" "sim"

let stas = ["sta1";"sta2"; "sta3"; "sta4"; "sta5"; "sta6"; "sta7"; "sta8"; "sta9";]

let mk_algo_sta = mk_list string "algo" stas

let n = 8388608

let mk_n = mk int "n" n

let sz_of_sta s = int_of_string (String.sub s 3 (String.length s - 3))

let formatter =
 Env.format (Env.(
  [
   ("n", Format_custom (fun n -> sprintf "%s" n));
   ("proc", Format_custom (fun n -> sprintf "Nb. cores %s" n));
   ("algo", Format_custom (fun n -> sprintf "%s" (
     if n = "dyn" then "Incounter"
     else if n = "sim" then "Fetch & Add"
     else (sprintf "SNZI depth=%d" (sz_of_sta n)))));
   ("threshold", Format_custom (fun n -> "" (*sprintf "threshold=%s" n *) ));
   ]
  ))

let binaries = ["counters.sim";"counters.dyn";] @ (List.map (fun n -> "counters."^n) stas)

let threshold_of p = (40000 / p)

let mk_threshold = mk int "threshold" (40000 / nb_proc)

let mk_bench_fanin = mk string "bench" "fanin"

let mk_bench_indegree2 = mk string "bench" "indegree2"

let mk_thresholds_and_procs procs =
  let mk p = (mk int "proc" p) & (mk int "threshold" (threshold_of p))
  in
  List.fold_left (fun acc p -> acc ++ (mk p)) (mk (List.hd procs)) (List.tl procs)

(*****************************************************************************)
(** Threshold experiment *)

module ExpThreshold = struct

let name = "threshold"

let prog = "./counters.virtual"

let mk_thresholds = mk_list int "threshold" [1;10;100;1000;10000;100000;1000000;]

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_proc
    & mk_bench_fanin
    & ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_thresholds) )
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
    Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [ Axis.Is_log false;] ]);
      Formatter formatter;
      Charts mk_unit;
      Series mk_proc;
      X ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_thresholds) );
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Proc experiment *)

module ExpProc = struct

let name = "proc"

let prog = "./counters.virtual"

let procs = [1;2;3;4;5;6;7;8;9;10;20;30;40;]
let procs = if nb_proc = 40 then procs else procs @ [nb_proc]

let mk_procs = mk_list int "proc" procs

let mk_all = (mk_algo_sim & mk_procs) ++ (mk_algo_sta & mk_procs) ++ (mk_algo_dyn & (mk_thresholds_and_procs procs))

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_bench_fanin
    & mk_all
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log false;] ]);
       Formatter formatter;
       Charts mk_unit;
      Series ( mk_algo_sim ++ mk_algo_sta ++ mk_algo_dyn );
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Size experiment *)

module ExpSize = struct

let name = "size"

let prog = "./counters.virtual"

let ns = Array.to_list (Array.init 7 (fun i -> (1 lsl (i+3)) * 1048576))

let mk_ns = mk_list int "n" ns

let procs = [1;10;20;30;40;]
let procs = if nb_proc = 40 then procs else procs @ [nb_proc]

let mk_procs = mk_list int "proc" procs

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_bench_fanin
    & mk_algo_dyn
    & (mk_thresholds_and_procs procs)
    & mk_ns)]))

let check = nothing  (* do something here *)

let plot() =
    Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Bottom_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true;
         Y_axis [Axis.Lower (Some 0.); Axis.Upper(Some 3500000.); ] ]);
       Formatter formatter;
       Charts mk_unit;
      Series (mk_algo_dyn & mk_procs);
      X mk_ns;
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Workload fanin experiment *)

module ExpWorkloadFanin = struct

let name = "workload_fanin"

let prog = "./counters.virtual"

let workloads = [1;10;100;1000;10000;100000;]

let mk_workloads = mk_list int "workload" workloads

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_proc
    & mk_bench_fanin
    & ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_threshold) )
    & mk_workloads
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Chart.Title "";
      Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         X_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log true;];
         Y_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log false;] ]);
       Formatter formatter;
       Charts mk_proc;
      Series ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_threshold) );
      X mk_workloads;
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Workload indegree2 experiment *)

module ExpWorkloadIndegree2 = struct

let name = "workload_indegree2"

let prog = "./counters.virtual"

let workloads = [1;10;100;1000;10000;100000;]

let mk_workloads = mk_list int "workload" workloads

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_proc
    & (mk_bench_indegree2)
    & ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_threshold) )
    & mk_workloads
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
        Chart.Title "";
        Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         X_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log true;];
         Y_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log false;] ]);
       Formatter formatter;
       Charts mk_proc;
      Series ( mk_algo_sim ++ mk_algo_sta ++ (mk_algo_dyn & mk_threshold) );
      X mk_workloads;
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Indegree2 experiment *)

module ExpIndegree2 = struct

let name = "indegree2"

let prog = "./counters.virtual"

let procs = [1;10;20;30;40;]
let procs = if nb_proc = 40 then procs else procs @ [nb_proc]

let mk_procs = mk_list int "proc" procs

let stas = ["sta2"; "sta4"; (*"sta8"; "sta9"; "sta10"; *)]

let mk_algo_sta = mk_list string "algo" stas
                       
let mk_all = (mk_algo_sim & mk_procs) ++ (mk_algo_sta & mk_procs) ++ (mk_algo_dyn & (mk_thresholds_and_procs procs))
                                                                       
let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_bench_indegree2
    & mk_all
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [(*Axis.Lower (Some 0.); Axis.Upper(Some 5000000.); *) Axis.Is_log false;] ]);
       Formatter formatter;
       Charts mk_unit;
      Series ( mk_algo_sim ++ mk_algo_sta ++ mk_algo_dyn );
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "Number of operations per second per core";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "threshold", ExpThreshold.all;
    "proc", ExpProc.all;
    "size", ExpSize.all;
    "indegree2", ExpIndegree2.all;
    "workload_fanin", ExpWorkloadFanin.all;
    "workload_indegree2", ExpWorkloadIndegree2.all;
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
