#!/usr/bin/env python3
"""
Convert README.md into a beautifully styled PDF named README.pdf using Python markdown and LibreOffice headless.
"""

import os
import subprocess
import markdown

with open("README.md", "r", encoding="utf-8") as f:
    md_content = f.read()

# Replace mermaid diagram with clean ASCII block for PDF rendering
mermaid_block = """```mermaid
graph TD
    subgraph "Phase 1: Pure Interpreter (Flex + Bison only — 60%)"
        P1_SRC[Source Code .edge] --> P1_FLEX[Flex Scanner: lexer_interp.l]
        P1_FLEX --> P1_BISON[Bison Parser: interpreter.y]
        P1_BISON --> P1_EXEC[Direct C++ Semantic Action Execution]
        P1_EXEC --> P1_OUT[stdout Output]
    end

    subgraph "Phase 2: Ahead-Of-Time Compiler (LLVM + Clang — 40%)"
        P2_SRC[Source Code .edge] --> P2_FLEX[Flex Scanner: lexer.l]
        P2_FLEX --> P2_BISON[Bison Parser: parser.y]
        P2_BISON --> P2_AST[Abstract Syntax Tree AST.h]
        P2_AST --> P2_SEM[Semantic Analysis Semantic.cpp]
        P2_SEM --> P2_OPT[AST Optimizations DCE & Dead Branches]
        P2_OPT --> P2_LLVM[LLVM IR Code Generation codegen.cpp]
        P2_LLVM --> P2_IR[LLVM IR .ll]
        P2_IR --> P2_CLANG[Clang AOT Compiler / Linker]
        P2_RT[OpenCV Vision Runtime libedgeruntime.a] --> P2_CLANG
        P2_CLANG --> P2_BIN[Native Linux Executable ELF]
    end
```"""

ascii_block = """```text
========================================================================================
                          EDGEFLOW COMPILER & INTERPRETER PIPELINE
========================================================================================

Phase 1: Pure Interpreter (Flex + Bison only — 60% Marks)
----------------------------------------------------------------------------------------
[Source Code .edge] ──> [Flex: lexer_interp.l] ──> [Bison: interpreter.y]
                                                            │
                                                            ▼ (Grammar Semantic Actions)
                                                    [In-Memory C++ Evaluation]
                                                            │
                                                            ▼
                                                    [Program Output: stdout]

Phase 2: Ahead-Of-Time (AOT) LLVM Compiler (40% Marks)
----------------------------------------------------------------------------------------
[Source Code .edge] ──> [Flex: lexer.l] ──> [Bison: parser.y]
                                                    │
                                                    ▼
                                            [AST Hierarchy (AST.h)]
                                                    │
                                                    ▼
                                            [Semantic Analysis (Semantic.cpp)]
                                                    │
                                                    ▼
                                            [AST Optimization (ast.cpp)]
                                                    │
                                                    ▼
                                            [LLVM IR CodeGen (codegen.cpp)]
                                                    │
                                                    ▼
                                            [LLVM IR (.ll file)]
                                                    │
                                                    ▼ (Clang++ Compiler / Linker)
    [OpenCV Runtime (libedgeruntime.a)] ──> [Native Linux ELF Binary]
                                                    │
                                                    ▼
                                            [Execution & Hardware Acceleration]
========================================================================================
```"""

md_content = md_content.replace(mermaid_block, ascii_block)

body_html = markdown.markdown(
    md_content,
    extensions=['tables', 'fenced_code']
)

full_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>EdgeFlow Project Documentation — README</title>
<style>
    @page {{
        size: A4;
        margin: 20mm 15mm 20mm 15mm;
        @bottom-right {{
            content: "Page " counter(page);
        }}
    }}
    body {{
        font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, Helvetica, Arial, sans-serif;
        color: #24292f;
        line-height: 1.55;
        font-size: 10.5pt;
        background-color: #ffffff;
        margin: 0;
        padding: 0;
    }}
    h1 {{
        color: #0969da;
        font-size: 22pt;
        border-bottom: 2px solid #0969da;
        padding-bottom: 8px;
        margin-top: 0;
        margin-bottom: 12px;
    }}
    h2 {{
        color: #1f2328;
        font-size: 14pt;
        border-bottom: 1px solid #d0d7de;
        padding-bottom: 5px;
        margin-top: 24px;
        margin-bottom: 12px;
        page-break-after: avoid;
    }}
    h3 {{
        color: #1f2328;
        font-size: 11.5pt;
        margin-top: 16px;
        margin-bottom: 8px;
        page-break-after: avoid;
    }}
    h4 {{
        font-size: 10.5pt;
        margin-top: 12px;
        margin-bottom: 6px;
        page-break-after: avoid;
    }}
    p, li {{
        line-height: 1.55;
    }}
    code {{
        font-family: 'DejaVu Sans Mono', 'Fira Code', 'Courier New', monospace;
        font-size: 9pt;
        background-color: #f6f8fa;
        padding: 2px 5px;
        border-radius: 4px;
        border: 1px solid #e1e4e8;
    }}
    pre {{
        background-color: #f6f8fa;
        border: 1px solid #d0d7de;
        border-radius: 6px;
        padding: 10px 14px;
        font-family: 'DejaVu Sans Mono', 'Fira Code', 'Courier New', monospace;
        font-size: 8.5pt;
        line-height: 1.4;
        overflow-x: auto;
        page-break-inside: avoid;
        margin: 10px 0;
    }}
    pre code {{
        background: none;
        padding: 0;
        border: none;
        font-size: inherit;
    }}
    table {{
        border-collapse: collapse;
        width: 100%;
        margin: 14px 0;
        font-size: 9.5pt;
        page-break-inside: avoid;
    }}
    th, td {{
        border: 1px solid #d0d7de;
        padding: 7px 10px;
        text-align: left;
        vertical-align: top;
    }}
    th {{
        background-color: #f6f8fa;
        font-weight: 600;
        color: #1f2328;
    }}
    tr:nth-child(even) {{
        background-color: #fafbfc;
    }}
    blockquote {{
        margin: 10px 0;
        padding: 8px 14px;
        color: #57606a;
        border-left: 4px solid #0969da;
        background-color: #f6f8fa;
        border-radius: 0 4px 4px 0;
        page-break-inside: avoid;
    }}
    hr {{
        border: none;
        border-top: 1px solid #d0d7de;
        margin: 20px 0;
    }}
    .badge {{
        display: inline-block;
        padding: 2px 8px;
        font-size: 8pt;
        font-weight: 600;
        border-radius: 10px;
        background-color: #2da44e;
        color: white;
    }}
</style>
</head>
<body>
{body_html}
</body>
</html>"""

with open("/tmp/readme_for_pdf.html", "w", encoding="utf-8") as f:
    f.write(full_html)

print("HTML written to /tmp/readme_for_pdf.html. Converting to PDF via LibreOffice...")
subprocess.run([
    "libreoffice",
    "--headless",
    "--convert-to",
    "pdf:writer_pdf_Export",
    "/tmp/readme_for_pdf.html",
    "--outdir",
    os.getcwd()
], check=True)

# LibreOffice creates readme_for_pdf.pdf in current working directory.
if os.path.exists("readme_for_pdf.pdf"):
    os.rename("readme_for_pdf.pdf", "README.pdf")
    print("Successfully generated README.pdf!")
else:
    print("Error: readme_for_pdf.pdf was not produced.")
