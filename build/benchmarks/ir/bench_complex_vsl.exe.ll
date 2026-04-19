; ModuleID = 'bench_complex.vsl'
source_filename = "bench_complex.vsl"

@fstr.lit = private unnamed_addr constant [6 x i8] c"item_\00", align 1
@fstr.lit.3 = private unnamed_addr constant [8 x i8] c"_value_\00", align 1
@fstr.lit.5 = private unnamed_addr constant [18 x i8] c"Sieve primes:    \00", align 1
@fstr.lit.7 = private unnamed_addr constant [18 x i8] c"Matrix corner:   \00", align 1
@fstr.lit.9 = private unnamed_addr constant [18 x i8] c"Tree sum:        \00", align 1
@fstr.lit.11 = private unnamed_addr constant [18 x i8] c"Collatz total:   \00", align 1
@fstr.lit.13 = private unnamed_addr constant [18 x i8] c"String len sum:  \00", align 1
@fstr.lit.15 = private unnamed_addr constant [18 x i8] c"Pi approx:       \00", align 1
@fstr.lit.17 = private unnamed_addr constant [18 x i8] c"Sort endpoints:  \00", align 1
@fstr.empty.18 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fstr.lit.19 = private unnamed_addr constant [18 x i8] c"Nested total:    \00", align 1
@str = private unnamed_addr constant [5 x i8] c"done\00", align 1

declare void @__visuall_print_str(ptr) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_int_to_str(i64) local_unnamed_addr

declare ptr @__visuall_float_to_str(double) local_unnamed_addr

declare double @__visuall_int_to_float(i64) local_unnamed_addr

declare ptr @__visuall_str_concat(ptr, ptr) local_unnamed_addr

declare i64 @__visuall_str_len(ptr) local_unnamed_addr

declare ptr @__visuall_list_new() local_unnamed_addr

declare void @__visuall_list_push(ptr, i64) local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define i64 @sieve(i64 %limit) local_unnamed_addr {
entry:
  %list.new = tail call ptr @__visuall_list_new()
  tail call void @__visuall_list_push(ptr %list.new, i64 1)
  %lt1159 = icmp sgt i64 %limit, 4
  br i1 %lt1159, label %while.body6, label %while.cond28.preheader

while.cond28.preheader:                           ; preds = %if.end, %entry
  %lt3362 = icmp sgt i64 %limit, 2
  br i1 %lt3362, label %while.body29, label %while.end30

while.body6:                                      ; preds = %entry, %if.end
  %mul61 = phi i64 [ %mul, %if.end ], [ 4, %entry ]
  %p.060 = phi i64 [ %add27, %if.end ], [ 2, %entry ]
  %idx.get = tail call i64 @__visuall_list_get(ptr %list.new, i64 %p.060)
  %eq = icmp eq i64 %idx.get, 1
  br i1 %eq, label %while.cond17, label %if.end

while.cond17:                                     ; preds = %while.body6, %while.cond17
  %j.0 = phi i64 [ %add25, %while.cond17 ], [ %mul61, %while.body6 ]
  %lt22 = icmp slt i64 %j.0, %limit
  %add25 = add i64 %j.0, %p.060
  br i1 %lt22, label %while.cond17, label %if.end

if.end:                                           ; preds = %while.cond17, %while.body6
  %add27 = add i64 %p.060, 1
  %mul = mul i64 %add27, %add27
  %lt11 = icmp slt i64 %mul, %limit
  br i1 %lt11, label %while.body6, label %while.cond28.preheader

while.body29:                                     ; preds = %while.cond28.preheader, %while.body29
  %k.064 = phi i64 [ %add43, %while.body29 ], [ 2, %while.cond28.preheader ]
  %count.063 = phi i64 [ %spec.select, %while.body29 ], [ 0, %while.cond28.preheader ]
  %idx.get36 = tail call i64 @__visuall_list_get(ptr %list.new, i64 %k.064)
  %eq37 = icmp eq i64 %idx.get36, 1
  %add40 = zext i1 %eq37 to i64
  %spec.select = add i64 %count.063, %add40
  %add43 = add nuw nsw i64 %k.064, 1
  %lt33 = icmp slt i64 %add43, %limit
  br i1 %lt33, label %while.body29, label %while.end30

while.end30:                                      ; preds = %while.body29, %while.cond28.preheader
  %count.0.lcssa = phi i64 [ 0, %while.cond28.preheader ], [ %spec.select, %while.body29 ]
  ret i64 %count.0.lcssa
}

define double @matrix_multiply(i64 %n) local_unnamed_addr {
entry:
  %list.new = tail call ptr @__visuall_list_new()
  tail call void @__visuall_list_push(ptr %list.new, i64 0)
  %list.new2 = tail call ptr @__visuall_list_new()
  tail call void @__visuall_list_push(ptr %list.new2, i64 0)
  %list.new3 = tail call ptr @__visuall_list_new()
  tail call void @__visuall_list_push(ptr %list.new3, i64 0)
  %lt13109 = icmp sgt i64 %n, 0
  br i1 %lt13109, label %while.body15.preheader, label %while.end33

while.body15.preheader:                           ; preds = %entry, %while.end16
  %i.0110 = phi i64 [ %add30, %while.end16 ], [ 0, %entry ]
  br label %while.body15

while.body15:                                     ; preds = %while.body15.preheader, %while.body15
  %j.0108 = phi i64 [ %add28, %while.body15 ], [ 0, %while.body15.preheader ]
  %add22 = add nuw i64 %j.0108, %i.0110
  %tofloat = tail call double @__visuall_int_to_float(i64 %add22)
  %sub = sub nsw i64 %i.0110, %j.0108
  %tofloat25 = tail call double @__visuall_int_to_float(i64 %sub)
  %add28 = add nuw nsw i64 %j.0108, 1
  %lt19 = icmp slt i64 %add28, %n
  br i1 %lt19, label %while.body15, label %while.end16

while.end16:                                      ; preds = %while.body15
  %add30 = add nuw nsw i64 %i.0110, 1
  %lt13 = icmp slt i64 %add30, %n
  br i1 %lt13, label %while.body15.preheader, label %while.cond44.preheader.lr.ph

while.cond44.preheader.lr.ph:                     ; preds = %while.end16, %while.end40
  %i.1116 = phi i64 [ %add71, %while.end40 ], [ 0, %while.end16 ]
  %mul54 = mul i64 %i.1116, %n
  br label %while.cond44.preheader

while.end33:                                      ; preds = %while.end40, %entry
  %mul = mul i64 %n, %n
  %idx.get73 = tail call i64 @__visuall_list_get(ptr %list.new3, i64 0)
  %sub78 = add i64 %mul, -1
  %idx.get79 = tail call i64 @__visuall_list_get(ptr %list.new3, i64 %sub78)
  %add80 = add i64 %idx.get79, %idx.get73
  %promo81 = sitofp i64 %add80 to double
  ret double %promo81

while.cond44.preheader:                           ; preds = %while.cond44.preheader.lr.ph, %while.end46
  %j37.0114 = phi i64 [ 0, %while.cond44.preheader.lr.ph ], [ %add69, %while.end46 ]
  br label %while.body45

while.end40:                                      ; preds = %while.end46
  %add71 = add nuw nsw i64 %i.1116, 1
  %lt36 = icmp slt i64 %add71, %n
  br i1 %lt36, label %while.cond44.preheader.lr.ph, label %while.end33

while.body45:                                     ; preds = %while.cond44.preheader, %while.body45
  %k.0112 = phi i64 [ 0, %while.cond44.preheader ], [ %add66, %while.body45 ]
  %add56 = add i64 %k.0112, %mul54
  %idx.get = tail call i64 @__visuall_list_get(ptr %list.new, i64 %add56)
  %mul60 = mul i64 %k.0112, %n
  %add62 = add i64 %mul60, %j37.0114
  %idx.get63 = tail call i64 @__visuall_list_get(ptr %list.new2, i64 %add62)
  %add66 = add nuw nsw i64 %k.0112, 1
  %lt49 = icmp slt i64 %add66, %n
  br i1 %lt49, label %while.body45, label %while.end46

while.end46:                                      ; preds = %while.body45
  %add69 = add nuw nsw i64 %j37.0114, 1
  %lt43 = icmp slt i64 %add69, %n
  br i1 %lt43, label %while.cond44.preheader, label %while.end40
}

; Function Attrs: nofree nosync nounwind memory(none)
define i64 @build_tree_sum(i64 %depth, i64 %val) local_unnamed_addr #0 {
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
define i64 @collatz_steps(i64 %n) local_unnamed_addr #1 {
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
  %fstr.cat = tail call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit)
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

define i64 @insertion_sort(i64 %size) local_unnamed_addr {
entry:
  %list.new = tail call ptr @__visuall_list_new()
  tail call void @__visuall_list_push(ptr %list.new, i64 0)
  %lt1055 = icmp sgt i64 %size, 1
  br i1 %lt1055, label %while.body15.preheader, label %while.end7

while.body15.preheader:                           ; preds = %entry, %while.end16
  %i.156 = phi i64 [ %add34, %while.end16 ], [ 1, %entry ]
  %idx.get = tail call i64 @__visuall_list_get(ptr %list.new, i64 %i.156)
  br label %while.body15

while.end7:                                       ; preds = %while.end16, %entry
  %idx.get36 = tail call i64 @__visuall_list_get(ptr %list.new, i64 0)
  %sub39 = add i64 %size, -1
  %idx.get40 = tail call i64 @__visuall_list_get(ptr %list.new, i64 %sub39)
  %add41 = add i64 %idx.get40, %idx.get36
  ret i64 %add41

while.body15:                                     ; preds = %while.body15.preheader, %if.end
  %j.054.in = phi i64 [ %j.054, %if.end ], [ %i.156, %while.body15.preheader ]
  %j.054 = add nsw i64 %j.054.in, -1
  %idx.get20 = tail call i64 @__visuall_list_get(ptr %list.new, i64 %j.054)
  %gt = icmp sgt i64 %idx.get20, %idx.get
  br i1 %gt, label %if.end, label %while.end16

while.end16:                                      ; preds = %while.body15, %if.end
  %add34 = add nuw nsw i64 %i.156, 1
  %lt10 = icmp slt i64 %add34, %size
  br i1 %lt10, label %while.body15.preheader, label %while.end7

if.end:                                           ; preds = %while.body15
  %idx.get24 = tail call i64 @__visuall_list_get(ptr %list.new, i64 %j.054)
  %ge = icmp sgt i64 %j.054.in, 1
  br i1 %ge, label %while.body15, label %while.end16
}

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @nested_loops(i64 %n) local_unnamed_addr #1 {
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

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init.1(ptr nonnull %gc.anchor)
  %list.new.i = call ptr @__visuall_list_new()
  call void @__visuall_list_push(ptr %list.new.i, i64 1)
  %idx.get.i = call i64 @__visuall_list_get(ptr %list.new.i, i64 2)
  %idx.get.i.1 = call i64 @__visuall_list_get(ptr %list.new.i, i64 3)
  %idx.get.i.2 = call i64 @__visuall_list_get(ptr %list.new.i, i64 4)
  %idx.get.i.3 = call i64 @__visuall_list_get(ptr %list.new.i, i64 5)
  %idx.get.i.4 = call i64 @__visuall_list_get(ptr %list.new.i, i64 6)
  %idx.get.i.5 = call i64 @__visuall_list_get(ptr %list.new.i, i64 7)
  %idx.get.i.6 = call i64 @__visuall_list_get(ptr %list.new.i, i64 8)
  %idx.get.i.7 = call i64 @__visuall_list_get(ptr %list.new.i, i64 9)
  br label %while.body29.i

while.body29.i:                                   ; preds = %entry, %while.body29.i
  %k.064.i = phi i64 [ %add43.i, %while.body29.i ], [ 2, %entry ]
  %idx.get36.i = call i64 @__visuall_list_get(ptr %list.new.i, i64 %k.064.i)
  %add43.i = add nuw nsw i64 %k.064.i, 1
  %lt33.i = icmp samesign ult i64 %k.064.i, 99
  br i1 %lt33.i, label %while.body29.i, label %while.body

while.body:                                       ; preds = %while.body29.i, %sieve.exit95
  %iter.0104 = phi i64 [ %add, %sieve.exit95 ], [ 0, %while.body29.i ]
  %list.new.i71 = call ptr @__visuall_list_new()
  call void @__visuall_list_push(ptr %list.new.i71, i64 1)
  br label %while.body6.i72

while.body6.i72:                                  ; preds = %while.body6.i72, %while.body
  %p.060.i74 = phi i64 [ %add27.i78, %while.body6.i72 ], [ 2, %while.body ]
  %idx.get.i75 = call i64 @__visuall_list_get(ptr %list.new.i71, i64 %p.060.i74)
  %add27.i78 = add nuw nsw i64 %p.060.i74, 1
  %mul.i79 = mul i64 %add27.i78, %add27.i78
  %lt11.i80 = icmp slt i64 %mul.i79, 100000
  br i1 %lt11.i80, label %while.body6.i72, label %while.body29.i82

while.body29.i82:                                 ; preds = %while.body6.i72, %while.body29.i82
  %k.064.i83 = phi i64 [ %add43.i89, %while.body29.i82 ], [ 2, %while.body6.i72 ]
  %count.063.i84 = phi i64 [ %spec.select.i88, %while.body29.i82 ], [ 0, %while.body6.i72 ]
  %idx.get36.i85 = call i64 @__visuall_list_get(ptr %list.new.i71, i64 %k.064.i83)
  %eq37.i86 = icmp eq i64 %idx.get36.i85, 1
  %add40.i87 = zext i1 %eq37.i86 to i64
  %spec.select.i88 = add i64 %count.063.i84, %add40.i87
  %add43.i89 = add nuw nsw i64 %k.064.i83, 1
  %lt33.i90 = icmp samesign ult i64 %k.064.i83, 99999
  br i1 %lt33.i90, label %while.body29.i82, label %sieve.exit95

sieve.exit95:                                     ; preds = %while.body29.i82
  %add = add nuw nsw i64 %iter.0104, 1
  %lt = icmp samesign ult i64 %iter.0104, 19
  br i1 %lt, label %while.body, label %while.end

while.end:                                        ; preds = %sieve.exit95
  %fstr.cat = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.5)
  %fstr.int = call ptr @__visuall_int_to_str(i64 %spec.select.i88)
  %fstr.cat5 = call ptr @__visuall_str_concat(ptr %fstr.cat, ptr %fstr.int)
  call void @__visuall_print_str(ptr %fstr.cat5)
  call void @__visuall_print_newline()
  %call11 = call double @matrix_multiply(i64 150)
  %call11.1 = call double @matrix_multiply(i64 150)
  %call11.2 = call double @matrix_multiply(i64 150)
  %call11.3 = call double @matrix_multiply(i64 150)
  %call11.4 = call double @matrix_multiply(i64 150)
  %fstr.cat14 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.7)
  %fstr.float = call ptr @__visuall_float_to_str(double %call11.4)
  %fstr.cat16 = call ptr @__visuall_str_concat(ptr %fstr.cat14, ptr %fstr.float)
  call void @__visuall_print_str(ptr %fstr.cat16)
  call void @__visuall_print_newline()
  %call17 = call i64 @build_tree_sum(i64 22, i64 1)
  %fstr.cat18 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.9)
  %fstr.int20 = call ptr @__visuall_int_to_str(i64 %call17)
  %fstr.cat21 = call ptr @__visuall_str_concat(ptr %fstr.cat18, ptr %fstr.int20)
  call void @__visuall_print_str(ptr %fstr.cat21)
  call void @__visuall_print_newline()
  br label %while.body23

while.body23:                                     ; preds = %while.end, %collatz_steps.exit
  %n.0107 = phi i64 [ 1, %while.end ], [ %add31, %collatz_steps.exit ]
  %r4.0106 = phi i64 [ 0, %while.end ], [ %add29, %collatz_steps.exit ]
  %ne.not13.i = icmp eq i64 %n.0107, 1
  br i1 %ne.not13.i, label %collatz_steps.exit, label %while.body.i

while.body.i:                                     ; preds = %while.body23, %while.body.i
  %n1.015.i = phi i64 [ %n1.1.i, %while.body.i ], [ %n.0107, %while.body23 ]
  %steps.014.i = phi i64 [ %add7.i, %while.body.i ], [ 0, %while.body23 ]
  %0 = and i64 %n1.015.i, 1
  %eq.i96 = icmp eq i64 %0, 0
  %idiv.i = ashr exact i64 %n1.015.i, 1
  %mul.i97 = mul i64 %n1.015.i, 3
  %add.i = add i64 %mul.i97, 1
  %n1.1.i = select i1 %eq.i96, i64 %idiv.i, i64 %add.i
  %add7.i = add i64 %steps.014.i, 1
  %ne.not.i = icmp eq i64 %n1.1.i, 1
  br i1 %ne.not.i, label %collatz_steps.exit, label %while.body.i

collatz_steps.exit:                               ; preds = %while.body.i, %while.body23
  %steps.0.lcssa.i = phi i64 [ 0, %while.body23 ], [ %add7.i, %while.body.i ]
  %add29 = add i64 %steps.0.lcssa.i, %r4.0106
  %add31 = add nuw nsw i64 %n.0107, 1
  %le = icmp samesign ult i64 %n.0107, 50000
  br i1 %le, label %while.body23, label %while.end24

while.end24:                                      ; preds = %collatz_steps.exit
  %fstr.cat32 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.11)
  %fstr.int34 = call ptr @__visuall_int_to_str(i64 %add29)
  %fstr.cat35 = call ptr @__visuall_str_concat(ptr %fstr.cat32, ptr %fstr.int34)
  call void @__visuall_print_str(ptr %fstr.cat35)
  call void @__visuall_print_newline()
  %call36 = call i64 @string_build(i64 200000)
  %fstr.cat37 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.13)
  %fstr.int39 = call ptr @__visuall_int_to_str(i64 %call36)
  %fstr.cat40 = call ptr @__visuall_str_concat(ptr %fstr.cat37, ptr %fstr.int39)
  call void @__visuall_print_str(ptr %fstr.cat40)
  call void @__visuall_print_newline()
  br label %while.body.i98

while.body.i98:                                   ; preds = %while.body.i98, %while.end24
  %i.017.i = phi i64 [ %add9.i, %while.body.i98 ], [ 0, %while.end24 ]
  %pi.016.i = phi double [ %fadd.i, %while.body.i98 ], [ 0.000000e+00, %while.end24 ]
  %sign.015.i = phi double [ %fmul.i, %while.body.i98 ], [ 1.000000e+00, %while.end24 ]
  %mul.i99 = shl nuw nsw i64 %i.017.i, 1
  %add.i100 = or disjoint i64 %mul.i99, 1
  %tofloat.i = call double @__visuall_int_to_float(i64 %add.i100)
  %fdiv.i = fdiv double %sign.015.i, %tofloat.i
  %fadd.i = fadd double %pi.016.i, %fdiv.i
  %fmul.i = fneg double %sign.015.i
  %add9.i = add nuw nsw i64 %i.017.i, 1
  %lt.i = icmp samesign ult i64 %i.017.i, 4999999
  br i1 %lt.i, label %while.body.i98, label %compute_pi.exit

compute_pi.exit:                                  ; preds = %while.body.i98
  %1 = fmul double %fadd.i, 4.000000e+00
  %fstr.cat42 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.15)
  %fstr.float44 = call ptr @__visuall_float_to_str(double %1)
  %fstr.cat45 = call ptr @__visuall_str_concat(ptr %fstr.cat42, ptr %fstr.float44)
  call void @__visuall_print_str(ptr %fstr.cat45)
  call void @__visuall_print_newline()
  %call51 = call i64 @insertion_sort(i64 5000)
  %call51.1 = call i64 @insertion_sort(i64 5000)
  %call51.2 = call i64 @insertion_sort(i64 5000)
  %fstr.cat54 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.17)
  %fstr.int56 = call ptr @__visuall_int_to_str(i64 %call51.2)
  %fstr.cat57 = call ptr @__visuall_str_concat(ptr %fstr.cat54, ptr %fstr.int56)
  call void @__visuall_print_str(ptr %fstr.cat57)
  call void @__visuall_print_newline()
  br label %while.body5.preheader.i

while.body5.preheader.i:                          ; preds = %while.end6.i, %compute_pi.exit
  %total.029.i = phi i64 [ %add.i102, %while.end6.i ], [ 0, %compute_pi.exit ]
  %i.028.i = phi i64 [ %add16.i, %while.end6.i ], [ 0, %compute_pi.exit ]
  br label %while.body5.i

while.body5.i:                                    ; preds = %while.body5.i, %while.body5.preheader.i
  %j.026.i = phi i64 [ %add14.i, %while.body5.i ], [ 0, %while.body5.preheader.i ]
  %total.125.i = phi i64 [ %add.i102, %while.body5.i ], [ %total.029.i, %while.body5.preheader.i ]
  %mul.i101 = mul nuw nsw i64 %j.026.i, %i.028.i
  %mod.i.urem = urem i64 %mul.i101, 97
  %add.i102 = add i64 %mod.i.urem, %total.125.i
  %add14.i = add nuw nsw i64 %j.026.i, 1
  %lt9.i = icmp samesign ult i64 %j.026.i, 2999
  br i1 %lt9.i, label %while.body5.i, label %while.end6.i

while.end6.i:                                     ; preds = %while.body5.i
  %add16.i = add nuw nsw i64 %i.028.i, 1
  %lt.i103 = icmp samesign ult i64 %i.028.i, 2999
  br i1 %lt.i103, label %while.body5.preheader.i, label %nested_loops.exit

nested_loops.exit:                                ; preds = %while.end6.i
  %fstr.cat59 = call ptr @__visuall_str_concat(ptr nonnull @fstr.empty.18, ptr nonnull @fstr.lit.19)
  %fstr.int61 = call ptr @__visuall_int_to_str(i64 %add.i102)
  %fstr.cat62 = call ptr @__visuall_str_concat(ptr %fstr.cat59, ptr %fstr.int61)
  call void @__visuall_print_str(ptr %fstr.cat62)
  call void @__visuall_print_newline()
  call void @__visuall_print_str(ptr nonnull @str)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

declare void @__visuall_gc_init.1(ptr) local_unnamed_addr

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define void @Node_init(ptr nocapture readnone %this, i64 %val) local_unnamed_addr #2 {
entry:
  ret void
}

attributes #0 = { nofree nosync nounwind memory(none) }
attributes #1 = { nofree norecurse nosync nounwind memory(none) }
attributes #2 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) }
