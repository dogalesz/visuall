; ModuleID = 'bench_primes.vsl'
source_filename = "bench_primes.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
is_prime.exit.i.1:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %if.end.i.i.2

if.end.i.i.2:                                     ; preds = %is_prime.exit.i.2.1, %is_prime.exit.i.1
  %n.013.i.2 = phi i64 [ 2, %is_prime.exit.i.1 ], [ %add7.i.2.1, %is_prime.exit.i.2.1 ]
  %count.012.i.2 = phi i64 [ 0, %is_prime.exit.i.1 ], [ %add.i.2.1, %is_prime.exit.i.2.1 ]
  %lt4.i.i.2 = icmp samesign ult i64 %n.013.i.2, 4
  %spec.select = select i1 %lt4.i.i.2, i64 1, i64 0
  %add.i.2 = add i64 %spec.select, %count.012.i.2
  %add7.i.2 = or disjoint i64 %n.013.i.2, 1
  %lt4.i.i.2.1 = icmp samesign ult i64 %n.013.i.2, 4
  %le.not27.i.i.2.1 = icmp samesign ult i64 %add7.i.2, 9
  %or.cond = select i1 %lt4.i.i.2.1, i1 true, i1 %le.not27.i.i.2.1
  br i1 %or.cond, label %is_prime.exit.i.2.1, label %while.body.i.i.2.1

while.body.i.i.2.1:                               ; preds = %if.end.i.i.2, %while.cond.i.i.2.1
  %d.028.i.i.2.1 = phi i64 [ %add.i.i.2.1, %while.cond.i.i.2.1 ], [ 3, %if.end.i.i.2 ]
  %mod15.i.i.2.1 = srem i64 %add7.i.2, %d.028.i.i.2.1
  %eq16.i.i.2.1 = icmp eq i64 %mod15.i.i.2.1, 0
  br i1 %eq16.i.i.2.1, label %is_prime.exit.i.2.1, label %while.cond.i.i.2.1

while.cond.i.i.2.1:                               ; preds = %while.body.i.i.2.1
  %add.i.i.2.1 = add i64 %d.028.i.i.2.1, 2
  %mul.i.i.2.1 = mul i64 %add.i.i.2.1, %add.i.i.2.1
  %le.not.i.i.2.1 = icmp sgt i64 %mul.i.i.2.1, %add7.i.2
  br i1 %le.not.i.i.2.1, label %is_prime.exit.i.2.1, label %while.body.i.i.2.1

is_prime.exit.i.2.1:                              ; preds = %while.body.i.i.2.1, %while.cond.i.i.2.1, %if.end.i.i.2
  %common.ret.op.i.i.2.1 = phi i64 [ 1, %if.end.i.i.2 ], [ 1, %while.cond.i.i.2.1 ], [ 0, %while.body.i.i.2.1 ]
  %add.i.2.1 = add i64 %common.ret.op.i.i.2.1, %add.i.2
  %add7.i.2.1 = add nuw nsw i64 %n.013.i.2, 2
  %exitcond.not.i.2.1 = icmp eq i64 %add7.i.2.1, 100000
  br i1 %exitcond.not.i.2.1, label %count_primes.exit.2, label %if.end.i.i.2

count_primes.exit.2:                              ; preds = %is_prime.exit.i.2.1
  call void @__visuall_print_int(i64 %add.i.2.1)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
