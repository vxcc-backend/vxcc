("amd64:cmov" (opt ((max_total_cmov_inline_cost 4) (consteval_iterations 6) (loop_simplify 1) (if_eval 1))) (type ("int" simple ((float 0) (size 4) (align 4)))) (type ("bool" simple ((float 0) (size 1) (align 1)))) (cu-block (((name "max2") (export 1)) (block ((args (("int" 1) ("int" 2))) (ops (((outs (("int" 0))) (name "imm") (params (("val" (uninit)))) (args ())) ((outs ()) (name "if") (params (("cond" (var 3)) ("then" (block ((args ()) (ops (((outs ()) (name "ret") (params ()) (args ((var 1)))))) (rets ())))) ("else" (block ((args ()) (ops (((outs ()) (name "ret") (params ()) (args ((var 2)))))) (rets ())))))) (args ())) ((outs (("bool" 3))) (name "sgt") (params (("a" (var 1)) ("b" (var 2)))) (args ())))) (rets (0)))))))