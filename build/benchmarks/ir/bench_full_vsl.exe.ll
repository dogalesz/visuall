; ModuleID = 'bench_full.vsl'
source_filename = "bench_full.vsl"

@fstr.lit = private unnamed_addr constant [6 x i8] c"item_\00", align 1
@fstr.lit.3 = private unnamed_addr constant [8 x i8] c"_value_\00", align 1
@fstr.lit.5 = private unnamed_addr constant [18 x i8] c"Primes < 100000: \00", align 1
@fstr.lit.7 = private unnamed_addr constant [18 x i8] c"Tree sum:        \00", align 1
@fstr.lit.9 = private unnamed_addr constant [18 x i8] c"Collatz total:   \00", align 1
@fstr.lit.11 = private unnamed_addr constant [18 x i8] c"String len sum:  \00", align 1
@fstr.lit.13 = private unnamed_addr constant [18 x i8] c"Pi approx:       \00", align 1
@fstr.lit.15 = private unnamed_addr constant [18 x i8] c"Nested total:    \00", align 1
@fstr.lit.17 = private unnamed_addr constant [18 x i8] c"Ackermann(3,11): \00", align 1
@fstr.lit.19 = private unnamed_addr constant [18 x i8] c"GCD sum:         \00", align 1
@fstr.lit.21 = private unnamed_addr constant [18 x i8] c"Fib sum:         \00", align 1
@fstr.empty.22 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fstr.lit.23 = private unnamed_addr constant [18 x i8] c"Dist sum:        \00", align 1
@str = private unnamed_addr constant [5 x i8] c"done\00", align 1

declare void @__visuall_print_str(ptr) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_int_to_str(i64) local_unnamed_addr

declare ptr @__visuall_float_to_str(double) local_unnamed_addr

declare double @__visuall_int_to_float(i64) local_unnamed_addr

declare ptr @__visuall_str_concat(ptr, ptr) local_unnamed_addr

declare i64 @__visuall_str_len(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define range(i64 0, 2) i64 @is_prime(i64 %n) local_unnamed_addr #0 {
entry:
  %lt = icmp slt i64 %n, 2
  br i1 %lt, label %common.ret, label %if.end

common.ret:                                       ; preds = %while.cond, %while.body, %while.cond.preheader, %if.end6, %if.end, %entry
  %common.ret.op = phi i64 [ 0, %entry ], [ 1, %if.end ], [ 0, %if.end6 ], [ 1, %while.cond.preheader ], [ 1, %while.cond ], [ 0, %while.body ]
  ret i64 %common.ret.op

if.end:                                           ; preds = %entry
  %lt4 = icmp samesign ult i64 %n, 4
  br i1 %lt4, label %common.ret, label %if.end6

if.end6:                                          ; preds = %if.end
  %mod = and i64 %n, 1
  %eq = icmp eq i64 %mod, 0
  br i1 %eq, label %common.ret, label %while.cond.preheader

while.cond.preheader:                             ; preds = %if.end6
  %le.not27 = icmp samesign ult i64 %n, 9
  br i1 %le.not27, label %common.ret, label %while.body

while.cond:                                       ; preds = %while.body
  %add = add i64 %d.028, 2
  %mul = mul i64 %add, %add
  %le.not = icmp sgt i64 %mul, %n
  br i1 %le.not, label %common.ret, label %while.body

while.body:                                       ; preds = %while.cond.preheader, %while.cond
  %d.028 = phi i64 [ %add, %while.cond ], [ 3, %while.cond.preheader ]
  %mod15 = srem i64 %n, %d.028
  %eq16 = icmp eq i64 %mod15, 0
  br i1 %eq16, label %common.ret, label %while.cond
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @count_primes(i64 %limit) local_unnamed_addr #0 {
entry:
  %lt12 = icmp sgt i64 %limit, 2
  br i1 %lt12, label %if.end.i, label %while.end

if.end.i:                                         ; preds = %entry, %is_prime.exit
  %n.014 = phi i64 [ %add7, %is_prime.exit ], [ 2, %entry ]
  %count.013 = phi i64 [ %add, %is_prime.exit ], [ 0, %entry ]
  %lt4.i = icmp samesign ult i64 %n.014, 4
  br i1 %lt4.i, label %is_prime.exit, label %if.end6.i

if.end6.i:                                        ; preds = %if.end.i
  %mod.i = and i64 %n.014, 1
  %eq.i = icmp eq i64 %mod.i, 0
  br i1 %eq.i, label %is_prime.exit, label %while.cond.preheader.i

while.cond.preheader.i:                           ; preds = %if.end6.i
  %le.not27.i = icmp samesign ult i64 %n.014, 9
  br i1 %le.not27.i, label %is_prime.exit, label %while.body.i

while.cond.i:                                     ; preds = %while.body.i
  %add.i = add i64 %d.028.i, 2
  %mul.i = mul i64 %add.i, %add.i
  %le.not.i = icmp sgt i64 %mul.i, %n.014
  br i1 %le.not.i, label %is_prime.exit, label %while.body.i

while.body.i:                                     ; preds = %while.cond.preheader.i, %while.cond.i
  %d.028.i = phi i64 [ %add.i, %while.cond.i ], [ 3, %while.cond.preheader.i ]
  %mod15.i = srem i64 %n.014, %d.028.i
  %eq16.i = icmp eq i64 %mod15.i, 0
  br i1 %eq16.i, label %is_prime.exit, label %while.cond.i

is_prime.exit:                                    ; preds = %while.cond.i, %while.body.i, %if.end.i, %if.end6.i, %while.cond.preheader.i
  %common.ret.op.i = phi i64 [ 1, %if.end.i ], [ 0, %if.end6.i ], [ 1, %while.cond.preheader.i ], [ 0, %while.body.i ], [ 1, %while.cond.i ]
  %add = add i64 %common.ret.op.i, %count.013
  %add7 = add nuw nsw i64 %n.014, 1
  %lt = icmp slt i64 %add7, %limit
  br i1 %lt, label %if.end.i, label %while.end

while.end:                                        ; preds = %is_prime.exit, %entry
  %count.0.lcssa = phi i64 [ 0, %entry ], [ %add, %is_prime.exit ]
  ret i64 %count.0.lcssa
}

; Function Attrs: nofree nosync nounwind memory(none)
define i64 @build_tree_sum(i64 %depth, i64 %val) local_unnamed_addr #1 {
entry:
  %le22 = icmp slt i64 %depth, 1
  br i1 %le22, label %common.ret, label %if.end

common.ret:                                       ; preds = %if.end, %entry
  %accumulator.tr.lcssa = phi i64 [ 0, %entry ], [ %add16, %if.end ]
  %val.tr.lcssa = phi i64 [ %val, %entry ], [ %add, %if.end ]
  %accumulator.ret.tr = add i64 %val.tr.lcssa, %accumulator.tr.lcssa
  ret i64 %accumulator.ret.tr

if.end:                                           ; preds = %entry, %if.end
  %val.tr25 = phi i64 [ %add, %if.end ], [ %val, %entry ]
  %depth.tr24 = phi i64 [ %sub, %if.end ], [ %depth, %entry ]
  %accumulator.tr23 = phi i64 [ %add16, %if.end ], [ 0, %entry ]
  %sub = add nsw i64 %depth.tr24, -1
  %mul = shl i64 %val.tr25, 1
  %call = tail call i64 @build_tree_sum(i64 %sub, i64 %mul)
  %add = or disjoint i64 %mul, 1
  %add14 = add i64 %val.tr25, %accumulator.tr23
  %add16 = add i64 %add14, %call
  %le = icmp samesign ult i64 %depth.tr24, 2
  br i1 %le, label %common.ret, label %if.end
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @collatz_steps(i64 %n) local_unnamed_addr #0 {
entry:
  %ne.not13 = icmp eq i64 %n, 1
  br i1 %ne.not13, label %while.end, label %while.body

while.body:                                       ; preds = %entry, %while.body
  %n1.015 = phi i64 [ %n1.1, %while.body ], [ %n, %entry ]
  %steps.014 = phi i64 [ %add7, %while.body ], [ 0, %entry ]
  %0 = and i64 %n1.015, 1
  %eq = icmp eq i64 %0, 0
  %idiv = ashr exact i64 %n1.015, 1
  %mul = mul i64 %n1.015, 3
  %add = add i64 %mul, 1
  %n1.1 = select i1 %eq, i64 %idiv, i64 %add
  %add7 = add i64 %steps.014, 1
  %ne.not = icmp eq i64 %n1.1, 1
  br i1 %ne.not, label %while.end, label %while.body

while.end:                                        ; preds = %while.body, %entry
  %steps.0.lcssa = phi i64 [ 0, %entry ], [ %add7, %while.body ]
  ret i64 %steps.0.lcssa
}

define i64 @string_build(i64 %iterations) local_unnamed_addr {
entry:
  %lt19 = icmp sgt i64 %iterations, 0
  br i1 %lt19, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %i.021 = phi i64 [ %add13, %while.body ], [ 0, %entry ]
  %total_len.020 = phi i64 [ %add, %while.body ], [ 0, %entry ]
  %fstr.cat = tail call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit)
  %fstr.int = tail call ptr @__visuall_int_to_str(i64 %i.021)
  %fstr.cat5 = tail call ptr @__visuall_str_concat(ptr %fstr.cat, ptr %fstr.int)
  %fstr.cat6 = tail call ptr @__visuall_str_concat(ptr %fstr.cat5, ptr nonnull @fstr.lit.3)
  %mul = mul i64 %i.021, 7
  %fstr.int8 = tail call ptr @__visuall_int_to_str(i64 %mul)
  %fstr.cat9 = tail call ptr @__visuall_str_concat(ptr %fstr.cat6, ptr %fstr.int8)
  %len = tail call i64 @__visuall_str_len(ptr %fstr.cat9)
  %add = add i64 %len, %total_len.020
  %add13 = add nuw nsw i64 %i.021, 1
  %lt = icmp slt i64 %add13, %iterations
  br i1 %lt, label %while.body, label %while.end

while.end:                                        ; preds = %while.body, %entry
  %total_len.0.lcssa = phi i64 [ 0, %entry ], [ %add, %while.body ]
  ret i64 %total_len.0.lcssa
}

define double @compute_pi(i64 %terms) local_unnamed_addr {
entry:
  %lt14 = icmp sgt i64 %terms, 0
  br i1 %lt14, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %i.017 = phi i64 [ %add9, %while.body ], [ 0, %entry ]
  %pi.016 = phi double [ %fadd, %while.body ], [ 0.000000e+00, %entry ]
  %sign.015 = phi double [ %fmul, %while.body ], [ 1.000000e+00, %entry ]
  %mul = shl nuw i64 %i.017, 1
  %add = or disjoint i64 %mul, 1
  %tofloat = tail call double @__visuall_int_to_float(i64 %add)
  %fdiv = fdiv double %sign.015, %tofloat
  %fadd = fadd double %pi.016, %fdiv
  %fmul = fneg double %sign.015
  %add9 = add nuw nsw i64 %i.017, 1
  %lt = icmp slt i64 %add9, %terms
  br i1 %lt, label %while.body, label %while.end.loopexit

while.end.loopexit:                               ; preds = %while.body
  %0 = fmul double %fadd, 4.000000e+00
  br label %while.end

while.end:                                        ; preds = %while.end.loopexit, %entry
  %pi.0.lcssa = phi double [ 0.000000e+00, %entry ], [ %0, %while.end.loopexit ]
  ret double %pi.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @nested_loops(i64 %n) local_unnamed_addr #0 {
entry:
  %lt27 = icmp sgt i64 %n, 0
  br i1 %lt27, label %while.body5.preheader, label %while.end

while.body5.preheader:                            ; preds = %entry, %while.end6
  %total.029 = phi i64 [ %add, %while.end6 ], [ 0, %entry ]
  %i.028 = phi i64 [ %add16, %while.end6 ], [ 0, %entry ]
  br label %while.body5

while.end:                                        ; preds = %while.end6, %entry
  %total.0.lcssa = phi i64 [ 0, %entry ], [ %add, %while.end6 ]
  ret i64 %total.0.lcssa

while.body5:                                      ; preds = %while.body5.preheader, %while.body5
  %j.026 = phi i64 [ %add14, %while.body5 ], [ 0, %while.body5.preheader ]
  %total.125 = phi i64 [ %add, %while.body5 ], [ %total.029, %while.body5.preheader ]
  %mul = mul i64 %j.026, %i.028
  %mod = srem i64 %mul, 97
  %add = add i64 %mod, %total.125
  %add14 = add nuw nsw i64 %j.026, 1
  %lt9 = icmp slt i64 %add14, %n
  br i1 %lt9, label %while.body5, label %while.end6

while.end6:                                       ; preds = %while.body5
  %add16 = add nuw nsw i64 %i.028, 1
  %lt = icmp slt i64 %add16, %n
  br i1 %lt, label %while.body5.preheader, label %while.end
}

; Function Attrs: nofree nosync nounwind memory(none)
define i64 @ackermann(i64 %m, i64 %n) local_unnamed_addr #1 {
entry:
  %eq22 = icmp eq i64 %m, 0
  br i1 %eq22, label %if.then, label %if.end

if.then:                                          ; preds = %tailrecurse.backedge, %entry
  %n.tr.lcssa = phi i64 [ %n, %entry ], [ %n.tr.be, %tailrecurse.backedge ]
  %add = add i64 %n.tr.lcssa, 1
  ret i64 %add

if.end:                                           ; preds = %entry, %tailrecurse.backedge
  %n.tr24 = phi i64 [ %n.tr.be, %tailrecurse.backedge ], [ %n, %entry ]
  %m.tr23 = phi i64 [ %m.tr.be, %tailrecurse.backedge ], [ %m, %entry ]
  %eq6 = icmp eq i64 %n.tr24, 0
  br i1 %eq6, label %tailrecurse.backedge, label %if.end9

tailrecurse.backedge:                             ; preds = %if.end, %if.end9
  %n.tr.be = phi i64 [ %call15, %if.end9 ], [ 1, %if.end ]
  %m.tr.be = add i64 %m.tr23, -1
  %eq = icmp eq i64 %m.tr.be, 0
  br i1 %eq, label %if.then, label %if.end

if.end9:                                          ; preds = %if.end
  %sub14 = add i64 %n.tr24, -1
  %call15 = tail call i64 @ackermann(i64 %m.tr23, i64 %sub14)
  br label %tailrecurse.backedge
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @gcd(i64 %a, i64 %b) local_unnamed_addr #0 {
entry:
  %ne.not12 = icmp eq i64 %b, 0
  br i1 %ne.not12, label %while.end, label %while.body

while.body:                                       ; preds = %entry, %while.body
  %a1.014 = phi i64 [ %b2.013, %while.body ], [ %a, %entry ]
  %b2.013 = phi i64 [ %mod, %while.body ], [ %b, %entry ]
  %mod = srem i64 %a1.014, %b2.013
  %ne.not = icmp eq i64 %mod, 0
  br i1 %ne.not, label %while.end, label %while.body

while.end:                                        ; preds = %while.body, %entry
  %a1.0.lcssa = phi i64 [ %a, %entry ], [ %b2.013, %while.body ]
  ret i64 %a1.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @gcd_sum(i64 %limit) local_unnamed_addr #0 {
entry:
  %le.not28 = icmp slt i64 %limit, 1
  br i1 %le.not28, label %while.end, label %while.cond5.preheader

while.cond5.preheader:                            ; preds = %entry, %while.end7
  %total.030 = phi i64 [ %add, %while.end7 ], [ 0, %entry ]
  %i.029 = phi i64 [ %add17, %while.end7 ], [ 1, %entry ]
  br label %while.body6

while.end:                                        ; preds = %while.end7, %entry
  %total.0.lcssa = phi i64 [ 0, %entry ], [ %add, %while.end7 ]
  ret i64 %total.0.lcssa

while.body6:                                      ; preds = %while.cond5.preheader, %gcd.exit
  %j.027 = phi i64 [ %i.029, %while.cond5.preheader ], [ %add15, %gcd.exit ]
  %total.126 = phi i64 [ %total.030, %while.cond5.preheader ], [ %add, %gcd.exit ]
  %ne.not12.i = icmp eq i64 %j.027, 0
  br i1 %ne.not12.i, label %gcd.exit, label %while.body.i

while.body.i:                                     ; preds = %while.body6, %while.body.i
  %a1.014.i = phi i64 [ %b2.013.i, %while.body.i ], [ %i.029, %while.body6 ]
  %b2.013.i = phi i64 [ %mod.i, %while.body.i ], [ %j.027, %while.body6 ]
  %mod.i = srem i64 %a1.014.i, %b2.013.i
  %ne.not.i = icmp eq i64 %mod.i, 0
  br i1 %ne.not.i, label %gcd.exit, label %while.body.i

gcd.exit:                                         ; preds = %while.body.i, %while.body6
  %a1.0.lcssa.i = phi i64 [ %i.029, %while.body6 ], [ %b2.013.i, %while.body.i ]
  %add = add i64 %a1.0.lcssa.i, %total.126
  %add15 = add i64 %j.027, 1
  %le10.not = icmp sgt i64 %add15, %limit
  br i1 %le10.not, label %while.end7, label %while.body6

while.end7:                                       ; preds = %gcd.exit
  %add17 = add i64 %i.029, 1
  %le.not = icmp sgt i64 %add17, %limit
  br i1 %le.not, label %while.end, label %while.cond5.preheader
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @fibonacci(i64 %n) local_unnamed_addr #0 {
entry:
  %le = icmp slt i64 %n, 2
  br i1 %le, label %common.ret, label %while.body

common.ret:                                       ; preds = %while.body, %entry
  %common.ret.op = phi i64 [ %n, %entry ], [ %add, %while.body ]
  ret i64 %common.ret.op

while.body:                                       ; preds = %entry, %while.body
  %i.021 = phi i64 [ %add12, %while.body ], [ 2, %entry ]
  %a.020 = phi i64 [ %b.019, %while.body ], [ 0, %entry ]
  %b.019 = phi i64 [ %add, %while.body ], [ 1, %entry ]
  %add = add i64 %a.020, %b.019
  %add12 = add i64 %i.021, 1
  %le6.not = icmp sgt i64 %add12, %n
  br i1 %le6.not, label %common.ret, label %while.body
}

define double @dist_bench(i64 %count) local_unnamed_addr {
entry:
  %lt21 = icmp sgt i64 %count, 0
  br i1 %lt21, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %i.023 = phi i64 [ %add, %while.body ], [ 0, %entry ]
  %total_dist.022 = phi double [ %fadd15, %while.body ], [ 0.000000e+00, %entry ]
  %mod = urem i64 %i.023, 100
  %tofloat = tail call double @__visuall_int_to_float(i64 %mod)
  %fsub = fadd double %tofloat, -5.000000e+01
  %mod6 = urem i64 %i.023, 73
  %tofloat7 = tail call double @__visuall_int_to_float(i64 %mod6)
  %fsub8 = fadd double %tofloat7, -3.650000e+01
  %fmul = fmul double %fsub, %fsub
  %fadd = fadd double %total_dist.022, %fmul
  %fmul14 = fmul double %fsub8, %fsub8
  %fadd15 = fadd double %fadd, %fmul14
  %add = add nuw nsw i64 %i.023, 1
  %lt = icmp slt i64 %add, %count
  br i1 %lt, label %while.body, label %while.end

while.end:                                        ; preds = %while.body, %entry
  %total_dist.0.lcssa = phi double [ 0.000000e+00, %entry ], [ %fadd15, %while.body ]
  ret double %total_dist.0.lcssa
}

define noundef i32 @main() local_unnamed_addr {
is_prime.exit.i.1:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init.1(ptr nonnull %gc.anchor)
  br label %if.end.i.i.2

if.end.i.i.2:                                     ; preds = %is_prime.exit.i.2, %is_prime.exit.i.1
  %n.014.i.2 = phi i64 [ %add7.i.2, %is_prime.exit.i.2 ], [ 2, %is_prime.exit.i.1 ]
  %count.013.i.2 = phi i64 [ %add.i.2, %is_prime.exit.i.2 ], [ 0, %is_prime.exit.i.1 ]
  %lt4.i.i.2 = icmp samesign ult i64 %n.014.i.2, 4
  br i1 %lt4.i.i.2, label %is_prime.exit.i.2, label %if.end6.i.i.2

if.end6.i.i.2:                                    ; preds = %if.end.i.i.2
  %mod.i.i.2 = and i64 %n.014.i.2, 1
  %eq.i.i.2 = icmp eq i64 %mod.i.i.2, 0
  br i1 %eq.i.i.2, label %is_prime.exit.i.2, label %while.cond.preheader.i.i.2

while.cond.preheader.i.i.2:                       ; preds = %if.end6.i.i.2
  %le.not27.i.i.2 = icmp samesign ult i64 %n.014.i.2, 9
  br i1 %le.not27.i.i.2, label %is_prime.exit.i.2, label %while.body.i.i.2

while.body.i.i.2:                                 ; preds = %while.cond.preheader.i.i.2, %while.cond.i.i.2
  %d.028.i.i.2 = phi i64 [ %add.i.i.2, %while.cond.i.i.2 ], [ 3, %while.cond.preheader.i.i.2 ]
  %mod15.i.i.2 = srem i64 %n.014.i.2, %d.028.i.i.2
  %eq16.i.i.2 = icmp eq i64 %mod15.i.i.2, 0
  br i1 %eq16.i.i.2, label %is_prime.exit.i.2, label %while.cond.i.i.2

while.cond.i.i.2:                                 ; preds = %while.body.i.i.2
  %add.i.i.2 = add i64 %d.028.i.i.2, 2
  %mul.i.i.2 = mul i64 %add.i.i.2, %add.i.i.2
  %le.not.i.i.2 = icmp sgt i64 %mul.i.i.2, %n.014.i.2
  br i1 %le.not.i.i.2, label %is_prime.exit.i.2, label %while.body.i.i.2

is_prime.exit.i.2:                                ; preds = %while.body.i.i.2, %while.cond.i.i.2, %while.cond.preheader.i.i.2, %if.end6.i.i.2, %if.end.i.i.2
  %common.ret.op.i.i.2 = phi i64 [ 1, %if.end.i.i.2 ], [ 0, %if.end6.i.i.2 ], [ 1, %while.cond.preheader.i.i.2 ], [ 1, %while.cond.i.i.2 ], [ 0, %while.body.i.i.2 ]
  %add.i.2 = add i64 %common.ret.op.i.i.2, %count.013.i.2
  %add7.i.2 = add nuw nsw i64 %n.014.i.2, 1
  %lt.i.2 = icmp samesign ult i64 %n.014.i.2, 99999
  br i1 %lt.i.2, label %if.end.i.i.2, label %count_primes.exit.2

count_primes.exit.2:                              ; preds = %is_prime.exit.i.2
  %fstr.cat = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.5)
  %fstr.int = call ptr @__visuall_int_to_str(i64 %add.i.2)
  %fstr.cat4 = call ptr @__visuall_str_concat(ptr %fstr.cat, ptr %fstr.int)
  call void @__visuall_print_str(ptr %fstr.cat4)
  call void @__visuall_print_newline()
  %call5 = call i64 @build_tree_sum(i64 22, i64 1)
  %fstr.cat6 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.7)
  %fstr.int8 = call ptr @__visuall_int_to_str(i64 %call5)
  %fstr.cat9 = call ptr @__visuall_str_concat(ptr %fstr.cat6, ptr %fstr.int8)
  call void @__visuall_print_str(ptr %fstr.cat9)
  call void @__visuall_print_newline()
  br label %while.body11

while.body11:                                     ; preds = %count_primes.exit.2, %collatz_steps.exit
  %r3.097 = phi i64 [ 0, %count_primes.exit.2 ], [ %add17, %collatz_steps.exit ]
  %n.096 = phi i64 [ 1, %count_primes.exit.2 ], [ %add19, %collatz_steps.exit ]
  %ne.not13.i = icmp eq i64 %n.096, 1
  br i1 %ne.not13.i, label %collatz_steps.exit, label %while.body.i

while.body.i:                                     ; preds = %while.body11, %while.body.i
  %n1.015.i = phi i64 [ %n1.1.i, %while.body.i ], [ %n.096, %while.body11 ]
  %steps.014.i = phi i64 [ %add7.i75, %while.body.i ], [ 0, %while.body11 ]
  %0 = and i64 %n1.015.i, 1
  %eq.i = icmp eq i64 %0, 0
  %idiv.i = ashr exact i64 %n1.015.i, 1
  %mul.i = mul i64 %n1.015.i, 3
  %add.i74 = add i64 %mul.i, 1
  %n1.1.i = select i1 %eq.i, i64 %idiv.i, i64 %add.i74
  %add7.i75 = add i64 %steps.014.i, 1
  %ne.not.i = icmp eq i64 %n1.1.i, 1
  br i1 %ne.not.i, label %collatz_steps.exit, label %while.body.i

collatz_steps.exit:                               ; preds = %while.body.i, %while.body11
  %steps.0.lcssa.i = phi i64 [ 0, %while.body11 ], [ %add7.i75, %while.body.i ]
  %add17 = add i64 %steps.0.lcssa.i, %r3.097
  %add19 = add nuw nsw i64 %n.096, 1
  %le = icmp samesign ult i64 %n.096, 100000
  br i1 %le, label %while.body11, label %while.end12

while.end12:                                      ; preds = %collatz_steps.exit
  %fstr.cat20 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.9)
  %fstr.int22 = call ptr @__visuall_int_to_str(i64 %add17)
  %fstr.cat23 = call ptr @__visuall_str_concat(ptr %fstr.cat20, ptr %fstr.int22)
  call void @__visuall_print_str(ptr %fstr.cat23)
  call void @__visuall_print_newline()
  %call24 = call i64 @string_build(i64 200000)
  %fstr.cat25 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.11)
  %fstr.int27 = call ptr @__visuall_int_to_str(i64 %call24)
  %fstr.cat28 = call ptr @__visuall_str_concat(ptr %fstr.cat25, ptr %fstr.int27)
  call void @__visuall_print_str(ptr %fstr.cat28)
  call void @__visuall_print_newline()
  br label %while.body.i76

while.body.i76:                                   ; preds = %while.body.i76, %while.end12
  %i.017.i = phi i64 [ %add9.i, %while.body.i76 ], [ 0, %while.end12 ]
  %pi.016.i = phi double [ %fadd.i, %while.body.i76 ], [ 0.000000e+00, %while.end12 ]
  %sign.015.i = phi double [ %fmul.i, %while.body.i76 ], [ 1.000000e+00, %while.end12 ]
  %mul.i77 = shl nuw nsw i64 %i.017.i, 1
  %add.i78 = or disjoint i64 %mul.i77, 1
  %tofloat.i = call double @__visuall_int_to_float(i64 %add.i78)
  %fdiv.i = fdiv double %sign.015.i, %tofloat.i
  %fadd.i = fadd double %pi.016.i, %fdiv.i
  %fmul.i = fneg double %sign.015.i
  %add9.i = add nuw nsw i64 %i.017.i, 1
  %lt.i79 = icmp samesign ult i64 %i.017.i, 9999999
  br i1 %lt.i79, label %while.body.i76, label %compute_pi.exit

compute_pi.exit:                                  ; preds = %while.body.i76
  %1 = fmul double %fadd.i, 4.000000e+00
  %fstr.cat30 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.13)
  %fstr.float = call ptr @__visuall_float_to_str(double %1)
  %fstr.cat32 = call ptr @__visuall_str_concat(ptr %fstr.cat30, ptr %fstr.float)
  call void @__visuall_print_str(ptr %fstr.cat32)
  call void @__visuall_print_newline()
  br label %while.body5.preheader.i

while.body5.preheader.i:                          ; preds = %while.end6.i, %compute_pi.exit
  %total.029.i = phi i64 [ %add.i81, %while.end6.i ], [ 0, %compute_pi.exit ]
  %i.028.i = phi i64 [ %add16.i, %while.end6.i ], [ 0, %compute_pi.exit ]
  br label %while.body5.i

while.body5.i:                                    ; preds = %while.body5.i, %while.body5.preheader.i
  %j.026.i = phi i64 [ %add14.i, %while.body5.i ], [ 0, %while.body5.preheader.i ]
  %total.125.i = phi i64 [ %add.i81, %while.body5.i ], [ %total.029.i, %while.body5.preheader.i ]
  %mul.i80 = mul nuw nsw i64 %j.026.i, %i.028.i
  %mod.i.urem = urem i64 %mul.i80, 97
  %add.i81 = add i64 %mod.i.urem, %total.125.i
  %add14.i = add nuw nsw i64 %j.026.i, 1
  %lt9.i = icmp samesign ult i64 %j.026.i, 1999
  br i1 %lt9.i, label %while.body5.i, label %while.end6.i

while.end6.i:                                     ; preds = %while.body5.i
  %add16.i = add nuw nsw i64 %i.028.i, 1
  %lt.i82 = icmp samesign ult i64 %i.028.i, 1999
  br i1 %lt.i82, label %while.body5.preheader.i, label %nested_loops.exit

nested_loops.exit:                                ; preds = %while.end6.i
  %fstr.cat34 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.15)
  %fstr.int36 = call ptr @__visuall_int_to_str(i64 %add.i81)
  %fstr.cat37 = call ptr @__visuall_str_concat(ptr %fstr.cat34, ptr %fstr.int36)
  call void @__visuall_print_str(ptr %fstr.cat37)
  call void @__visuall_print_newline()
  %call38 = call i64 @ackermann(i64 3, i64 11)
  %fstr.cat39 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.17)
  %fstr.int41 = call ptr @__visuall_int_to_str(i64 %call38)
  %fstr.cat42 = call ptr @__visuall_str_concat(ptr %fstr.cat39, ptr %fstr.int41)
  call void @__visuall_print_str(ptr %fstr.cat42)
  call void @__visuall_print_newline()
  br label %while.cond5.preheader.i

while.cond5.preheader.i:                          ; preds = %while.end7.i, %nested_loops.exit
  %total.030.i = phi i64 [ %add.i85, %while.end7.i ], [ 0, %nested_loops.exit ]
  %i.029.i = phi i64 [ %add17.i, %while.end7.i ], [ 1, %nested_loops.exit ]
  br label %while.body.i.i83.preheader

while.body.i.i83.preheader:                       ; preds = %while.cond5.preheader.i, %gcd.exit.i
  %j.027.i = phi i64 [ %i.029.i, %while.cond5.preheader.i ], [ %add15.i, %gcd.exit.i ]
  %total.126.i = phi i64 [ %total.030.i, %while.cond5.preheader.i ], [ %add.i85, %gcd.exit.i ]
  br label %while.body.i.i83

while.body.i.i83:                                 ; preds = %while.body.i.i83.preheader, %while.body.i.i83
  %a1.014.i.i = phi i64 [ %b2.013.i.i, %while.body.i.i83 ], [ %i.029.i, %while.body.i.i83.preheader ]
  %b2.013.i.i = phi i64 [ %mod.i.i84, %while.body.i.i83 ], [ %j.027.i, %while.body.i.i83.preheader ]
  %mod.i.i84 = srem i64 %a1.014.i.i, %b2.013.i.i
  %ne.not.i.i = icmp eq i64 %mod.i.i84, 0
  br i1 %ne.not.i.i, label %gcd.exit.i, label %while.body.i.i83

gcd.exit.i:                                       ; preds = %while.body.i.i83
  %add.i85 = add i64 %b2.013.i.i, %total.126.i
  %add15.i = add nuw nsw i64 %j.027.i, 1
  %le10.not.i = icmp samesign ugt i64 %j.027.i, 1999
  br i1 %le10.not.i, label %while.end7.i, label %while.body.i.i83.preheader

while.end7.i:                                     ; preds = %gcd.exit.i
  %add17.i = add nuw nsw i64 %i.029.i, 1
  %le.not.i = icmp samesign ugt i64 %i.029.i, 1999
  br i1 %le.not.i, label %gcd_sum.exit, label %while.cond5.preheader.i

gcd_sum.exit:                                     ; preds = %while.end7.i
  %fstr.cat44 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.19)
  %fstr.int46 = call ptr @__visuall_int_to_str(i64 %add.i85)
  %fstr.cat47 = call ptr @__visuall_str_concat(ptr %fstr.cat44, ptr %fstr.int46)
  call void @__visuall_print_str(ptr %fstr.cat47)
  call void @__visuall_print_newline()
  %fstr.cat58 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.21)
  %fstr.int60 = call ptr @__visuall_int_to_str(i64 4613732500000)
  %fstr.cat61 = call ptr @__visuall_str_concat(ptr %fstr.cat58, ptr %fstr.int60)
  call void @__visuall_print_str(ptr %fstr.cat61)
  call void @__visuall_print_newline()
  br label %while.body.i88

while.body.i88:                                   ; preds = %while.body.i88, %gcd_sum.exit
  %i.023.i = phi i64 [ %add.i93, %while.body.i88 ], [ 0, %gcd_sum.exit ]
  %total_dist.022.i = phi double [ %fadd15.i, %while.body.i88 ], [ 0.000000e+00, %gcd_sum.exit ]
  %mod.i89.lhs.trunc = trunc i64 %i.023.i to i32
  %mod.i89100 = urem i32 %mod.i89.lhs.trunc, 100
  %mod.i89.zext = zext nneg i32 %mod.i89100 to i64
  %tofloat.i90 = call double @__visuall_int_to_float(i64 %mod.i89.zext)
  %fsub.i = fadd double %tofloat.i90, -5.000000e+01
  %mod6.i.lhs.trunc = trunc i64 %i.023.i to i32
  %mod6.i101 = urem i32 %mod6.i.lhs.trunc, 73
  %mod6.i.zext = zext nneg i32 %mod6.i101 to i64
  %tofloat7.i = call double @__visuall_int_to_float(i64 %mod6.i.zext)
  %fsub8.i = fadd double %tofloat7.i, -3.650000e+01
  %fmul.i91 = fmul double %fsub.i, %fsub.i
  %fadd.i92 = fadd double %total_dist.022.i, %fmul.i91
  %fmul14.i = fmul double %fsub8.i, %fsub8.i
  %fadd15.i = fadd double %fadd.i92, %fmul14.i
  %add.i93 = add nuw nsw i64 %i.023.i, 1
  %lt.i94 = icmp samesign ult i64 %i.023.i, 999999
  br i1 %lt.i94, label %while.body.i88, label %dist_bench.exit

dist_bench.exit:                                  ; preds = %while.body.i88
  %fstr.cat63 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.22, ptr nonnull @fstr.lit.23)
  %fstr.float65 = call ptr @__visuall_float_to_str(double %fadd15.i)
  %fstr.cat66 = call ptr @__visuall_str_concat(ptr %fstr.cat63, ptr %fstr.float65)
  call void @__visuall_print_str(ptr %fstr.cat66)
  call void @__visuall_print_newline()
  call void @__visuall_print_str(ptr nonnull @str)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

declare void @__visuall_gc_init.1(ptr) local_unnamed_addr

attributes #0 = { nofree norecurse nosync nounwind memory(none) }
attributes #1 = { nofree nosync nounwind memory(none) }
