("amd64:cmov" (opt ((max_total_cmov_inline_cost 4) (consteval_iterations 6) (loop_simplify 1) (if_eval 1))) (type ("int" simple ((float 0) (size 4) (align 4)))) (type ("int*" simple ((float 0) (size 8) (align 64)))) (type ("long" simple ((float 0) (size 8) (align 8)))) (cu-block (((name "ptrtest") (export 1)) (block ((args (("int*" 1))) (ops (((outs (("int" 0))) (name "imm") (params (("val" (uninit)))) (args ())) ((outs (("long" 2))) (name "imm") (params (("val" (int 2)))) (args ())) ((outs (("int*" 3))) (name "mul") (params (("a" (var 2)) ("b" (int 4)))) (args ())) ((outs (("int*" 4))) (name "add") (params (("a" (var 1)) ("b" (var 3)))) (args ())) ((outs (("int" 5))) (name "load") (params (("addr" (var 4)))) (args ())) ((outs ()) (name "ret") (params ()) (args ((var 5)))))) (rets (0)))))))