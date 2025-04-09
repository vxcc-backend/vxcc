// REALLY BAD AI GENERATED CODE

use std::collections::HashMap;
use std::fs;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;
use regex::Regex;
use lazy_static::lazy_static;

#[derive(Clone)]
struct Token {
    name: String,
    val: String,
}

struct Lexer {
    rules: Vec<(String, Regex)>,
    content: String,
    pos: usize,
}

impl Lexer {
    fn new(rules: Vec<(String, Regex)>, content: String) -> Self {
        Lexer {
            rules,
            content,
            pos: 0,
        }
    }
}

impl Iterator for Lexer {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos >= self.content.len() {
            return None;
        }

        for (name, re) in &self.rules {
            if let Some(mat) = re.find(&self.content[self.pos..]) {
                if mat.start() == 0 {
                    self.pos += mat.start() + mat.len();
                    return Some(Token {
                        name: name.clone(),
                        val: mat.as_str().to_string(),
                    });
                }
            }
        }

        None
    }
}

fn escape(name: &str) -> String {
    lazy_static! {
        static ref KEYWORDS: Vec<&'static str> = vec![
            "volatile", "for", "while", "do", "break", "continue", "goto", "static", "inline",
            "extern",
        ];
    }

    if KEYWORDS.contains(&name) {
        format!("_{}", name)
    } else {
        name.to_string()
    }
}

fn popty(out: &mut Vec<Token>, ty: &str) -> Token {
    if out.is_empty() {
        panic!("Expected {} but got nothing", ty);
    }

    let v = out.remove(0);
    if v.name != ty {
        panic!("Expected {} but got {}", ty, v.name);
    }
    v
}

fn until(out: &mut Vec<Token>, ty: &str) -> Vec<Token> {
    let mut res = Vec::new();
    while !out.is_empty() {
        let a = out.remove(0);
        if a.name == ty {
            break;
        }
        res.push(a);
    }
    res
}

fn bundle(tks: Vec<Token>) -> Vec<Token> {
    let mut res = Vec::new();
    let mut colon = false;
    for tk in tks {
        if tk.name == ":" {
            colon = true;
        } else {
            if colon {
                if let Some(Token { .. }) = res.last_mut() {
                    if let Some(last) = res.pop() {
                        res.push(Token {
                            name: "list".to_string(),
                            val: format!("{},{}", last.val, tk.val),
                        });
                    }
                }
                colon = false;
            } else {
                res.push(tk);
            }
        }
    }
    res
}

fn chunk(tks: Vec<Token>, count: usize) -> Vec<Vec<Token>> {
    let mut res = Vec::new();
    let mut tks = tks;
    while !tks.is_empty() {
        let mut x = Vec::new();
        for _ in 0..count {
            if tks.len() > 0 {
                x.push(tks.remove(0));
            } else {
                break;
            }
        }
        res.push(x);
    }
    res
}

fn immedv(tks: Vec<Token>) -> Vec<String> {
    tks.into_iter().map(|tk| tk.val).collect()
}

fn parse(out: &mut Vec<Token>, file_header: &mut BufWriter<File>, file_source: &mut BufWriter<File>) {
    let ty = escape(&popty(out, "VARIABLE").val);
    if ty == "include" {
        let f = popty(out, "STRING").val;
        writeln!(file_header, "#include {}", f).unwrap();
    } else {
        let name = escape(&popty(out, "VARIABLE").val);
        popty(out, "(");

        let mut args = Vec::new();
        while !out.is_empty() && out[0].name == "VARIABLE" {
            let n = popty(out, "VARIABLE").val;
            let ty = until(out, ";");
            args.push((n, ty));
        }

        writeln!(file_header, "// Table {}", name).unwrap();
        writeln!(file_header, "typedef struct {{\n").unwrap();
        for arg in &args {
            writeln!(file_header, "  struct {{").unwrap();
            let mut enum_val = 'a';
            for ty in &arg.1 {
                if ty.name == "?" {
                    writeln!(file_header, "    int set;").unwrap();
                } else {
                    let x = if ty.val == "bool" {
                        "int".to_string()
                    } else if ty.val == "byte" {
                        "uint8_t".to_string()
                    } else if ty.val == "str" {
                        "const char *".to_string()
                    } else {
                        ty.val.clone()
                    };
                    writeln!(file_header, "    {} {};", x, enum_val).unwrap();
                    enum_val = ((enum_val as u8) + 1) as char;
                }
            }
            writeln!(file_header, "  }} {};", escape(&arg.0)).unwrap();
        }
        writeln!(file_header, "}} {}__entry;\n", name).unwrap();

        popty(out, ")");
        let mut entry_prefix = format!("{}_", name);
        if !out.is_empty() && out[0].val == "enum_entry_prefix" {
            out.remove(0);
            entry_prefix = out.remove(0).val.replace("\"", "");
        }

        popty(out, "{");

        let mut options = HashMap::new();
        while !out.is_empty() && out[0].val == "entry" {
            popty(out, "VARIABLE");
            let nam = escape(&popty(out, "VARIABLE").val);
            let values = chunk(bundle(until(out, ";")), 2)
                .into_iter()
                .map(|x| {
                    let mut x = x;
                    let first = x.remove(0).val;
                    (first, immedv(x))
                })
                .collect::<HashMap<_, _>>();
            for arg in &args {
                if arg.1[0].name != "?" && !values.contains_key(&arg.0) {
                    panic!("non-optional field {} not set", arg.0);
                }
            }
            options.insert(nam, values);
        }

        popty(out, "}");

        writeln!(file_header, "#define {}__len ({})\n", name, options.len()).unwrap();
        writeln!(
            file_header,
            "extern {}__entry {}__entries[{}__len];\n",
            name, name, name
        )
        .unwrap();

        writeln!(file_header, "typedef enum {{\n").unwrap();
        for (i, key) in options.keys().enumerate() {
            writeln!(file_header, "  {}{} = {},\n", entry_prefix, key, i).unwrap();
        }
        writeln!(file_header, "}} {};\n", name).unwrap();

        writeln!(file_source, "// Table {}\n", name).unwrap();
        writeln!(file_source, "{}__entry {}__entries[{}__len] = {{\n", name, name, name).unwrap();

        for (key, val) in options {
            writeln!(file_source, "  [{}{}] = ({}__entry) {{\n", entry_prefix, key, name).unwrap();
            for member in &args {
                let member_key = &member.0;
                let member_types = &member.1;
                if let Some(imm) = val.get(member_key) {
                    if member_types[0].name == "?" {
                        writeln!(file_source, "    .{} .set = 1,\n", escape(member_key)).unwrap();
                    }

                    let mut enum_val = 'a';
                    for x in imm {
                        writeln!(
                            file_source,
                            "    .{} .{} = {},\n",
                            escape(member_key),
                            enum_val,
                            x
                        )
                        .unwrap();
                        enum_val = ((enum_val as u8) + 1) as char;
                    }
                } else {
                    writeln!(file_source, "    .{} .set = 0,\n", escape(member_key)).unwrap();
                }
            }
            writeln!(file_source, "  }},\n").unwrap();
        }

        writeln!(file_source, "}};\n\n").unwrap();
    }
}

fn lex(content: String) -> Vec<Token> {
    lazy_static! {
        static ref RULES: Vec<(String, Regex)> = vec![
            ("VARIABLE".to_string(), Regex::new(r"[a-zA-Z_$]+[a-zA-Z0-9_$]*").unwrap()),
            ("NUMBER".to_string(), Regex::new(r"(0b)?[0-9]+").unwrap()),
            ("SPACE".to_string(), Regex::new(r"\s+").unwrap()),
            ("COMMENT".to_string(), Regex::new(r"\#.*").unwrap()),
            ("STRING".to_string(), Regex::new("\".*\"").unwrap()),
            (":".to_string(), Regex::new(r"\:").unwrap()),
            ("?".to_string(), Regex::new(r"\?").unwrap()),
            ("<".to_string(), Regex::new(r"\<").unwrap()),
            (">".to_string(), Regex::new(r"\>").unwrap()),
            ("(".to_string(), Regex::new("\\(").unwrap()),
            (")".to_string(), Regex::new("\\)").unwrap()),
            (";".to_string(), Regex::new(r";").unwrap()),
            ("{".to_string(), Regex::new(r"\{").unwrap()),
            ("}".to_string(), Regex::new(r"\}").unwrap()),
            ("=".to_string(), Regex::new(r"=").unwrap()),
            ("*".to_string(), Regex::new(r"\*").unwrap()),
            (",".to_string(), Regex::new(r",").unwrap()),
        ];
    }

    Lexer::new(RULES.clone(), content)
        .filter(|token| token.name != "SPACE" && token.name != "COMMENT")
        .collect()
}

/// returns the generated C file path
pub fn generate(cdef_path: &str) -> String {
    let mut toks = lex(fs::read_to_string(cdef_path).unwrap());

    let file_header_path = format!("build/{}.h", cdef_path);
    let file_source_path = format!("build/{}.c", cdef_path);

    let file_header = File::create(file_header_path).unwrap();
    let mut file_header = BufWriter::new(file_header);
    let file_source = File::create(file_source_path.as_str()).unwrap();
    let mut file_source = BufWriter::new(file_source);

    let cdef_file_name = Path::new(cdef_path).file_name().unwrap().to_str().unwrap();
    let cdef_esc_path = cdef_path
        .replace(".", "_")
        .replace("/", "__")
        .replace("\\", "__")
        .replace("-", "_");

    writeln!(file_header, "#ifndef _CDEF_{}\n#define _CDEF_{}\n#include <stdint.h>\n#ifndef true\n# define true (1)\n #define false (0)\n#endif\n", cdef_esc_path, cdef_esc_path).unwrap();
    writeln!(file_source, "#include \"{}.h\"\n", cdef_file_name).unwrap();
    while !toks.is_empty() {
        parse(&mut toks, &mut file_header, &mut file_source);
    }
    writeln!(file_header, "\n#endif\n").unwrap();

    file_source_path
}
