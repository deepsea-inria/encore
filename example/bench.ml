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

let nb_proc = 40

let mk_proc = mk int "proc" nb_proc

let mk_algo_dyn = mk string "algo" "dyn"

let mk_algo_sim = mk string "algo" "sim"

let mk_n = mk int "n" 8388608

let mlog10 n = int_of_float(log10(float_of_int n))

let formatter =
 Env.format (Env.(
  [
   ("n", Format_custom (fun n -> sprintf "%s" n));
   ("proc", Format_custom (fun n -> sprintf "P=%s" n));
   ("algo", Format_custom (fun n -> sprintf "%s" (if n = "dyn" then "Ours" else "F&A")));
   ("threshold", Format_custom (fun n -> sprintf "T=10^%d" (mlog10 (int_of_string n))));
   ]
  ))

let binaries = ["counters.sim";"counters.dyn";]

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
    & ( mk_algo_sim ++ (mk_algo_dyn & mk_thresholds) )
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
    Mk_bar_plot.(call ([
      Bar_plot_opt Bar_plot.([
         X_titles_dir Vertical;
         Y_axis [Axis.Lower (Some 0.)] ]);
      Formatter formatter;
      Charts mk_unit;
      Series mk_proc;
      X ( mk_algo_sim ++ (mk_algo_dyn & mk_thresholds) );
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
      Y eval_nb_operations_per_second;
  ]))

let all () = select make run check plot

end

(*****************************************************************************)
(** Proc experiment *)

module ExpProc = struct

let name = "proc"

let prog = "./counters.virtual"

let mk_threshold = mk int "threshold" 1000

let mk_procs = mk_list int "proc" [1;10;20;30;40;]

let make() =
  build "." binaries arg_virtual_build

let run() =
  Mk_runs.(call (run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_procs
    & ( mk_algo_sim ++ (mk_algo_dyn & mk_threshold) )
    & mk_n)]))

let check = nothing  (* do something here *)

let plot() =
  Mk_scatter_plot.(call ([
    Chart_opt Chart.([
      Legend_opt Legend.([Legend_pos Top_right]);
      ]);
     Scatter_plot_opt Scatter_plot.([
         Draw_lines true; 
         Y_axis [Axis.Is_log false;] ]);
       Formatter formatter;
       Charts mk_unit;
      Series ( mk_algo_sim ++ (mk_algo_dyn & mk_threshold) );
      X mk_procs;
      Input (file_results name);
      Output (file_plots name);
      Y_label "nb_operations/second (per thread)";
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
  ]
  in
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
