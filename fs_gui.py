#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import subprocess
import threading
import os
import time

class FS_GUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Linux二级文件系统 - 可视化管理")
        self.root.geometry("1000x600")
        self.root.minsize(800, 500)
        
        self.current_user = ""
        self.current_path = "/"
        self.process = None
        self.output_buffer = []
        
        self.create_widgets()
        self.start_filesys()
    
    def create_widgets(self):
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.file_tab = ttk.Frame(self.notebook)
        self.trash_tab = ttk.Frame(self.notebook)
        self.log_tab = ttk.Frame(self.notebook)
        self.shell_tab = ttk.Frame(self.notebook)
        
        self.notebook.add(self.file_tab, text="文件管理")
        self.notebook.add(self.trash_tab, text="回收站")
        self.notebook.add(self.log_tab, text="系统日志")
        self.notebook.add(self.shell_tab, text="命令行")
        
        self.create_file_tab()
        self.create_trash_tab()
        self.create_log_tab()
        self.create_shell_tab()
        
        self.status_bar = ttk.Label(self.root, text="就绪", relief=tk.SUNKEN)
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)
    
    def create_file_tab(self):
        toolbar = ttk.Frame(self.file_tab)
        toolbar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        self.btn_new_file = ttk.Button(toolbar, text="新建文件", command=self.new_file)
        self.btn_new_file.pack(side=tk.LEFT, padx=2)
        
        self.btn_new_dir = ttk.Button(toolbar, text="新建目录", command=self.new_dir)
        self.btn_new_dir.pack(side=tk.LEFT, padx=2)
        
        self.btn_delete = ttk.Button(toolbar, text="删除", command=self.delete_item)
        self.btn_delete.pack(side=tk.LEFT, padx=2)
        
        self.btn_rename = ttk.Button(toolbar, text="重命名", command=self.rename_item)
        self.btn_rename.pack(side=tk.LEFT, padx=2)
        
        self.btn_open = ttk.Button(toolbar, text="打开", command=self.open_file)
        self.btn_open.pack(side=tk.LEFT, padx=2)
        
        self.btn_refresh = ttk.Button(toolbar, text="刷新", command=self.refresh_file_list)
        self.btn_refresh.pack(side=tk.RIGHT, padx=2)
        
        path_frame = ttk.Frame(self.file_tab)
        path_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=2)
        
        ttk.Label(path_frame, text="路径:").pack(side=tk.LEFT)
        self.path_var = tk.StringVar(value="/")
        self.path_entry = ttk.Entry(path_frame, textvariable=self.path_var, state="readonly")
        self.path_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        
        self.btn_cd_up = ttk.Button(path_frame, text="上级", command=self.cd_up)
        self.btn_cd_up.pack(side=tk.RIGHT)
        
        main_frame = ttk.Frame(self.file_tab)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.file_tree = ttk.Treeview(main_frame, columns=("inode", "size", "perm", "type"), show="headings")
        self.file_tree.heading("inode", text="Inode")
        self.file_tree.heading("size", text="大小")
        self.file_tree.heading("perm", text="权限")
        self.file_tree.heading("type", text="类型")
        
        self.file_tree.column("inode", width=80, anchor=tk.CENTER)
        self.file_tree.column("size", width=80, anchor=tk.CENTER)
        self.file_tree.column("perm", width=80, anchor=tk.CENTER)
        self.file_tree.column("type", width=60, anchor=tk.CENTER)
        
        scrollbar = ttk.Scrollbar(main_frame, orient=tk.VERTICAL, command=self.file_tree.yview)
        self.file_tree.configure(yscrollcommand=scrollbar.set)
        
        self.file_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.file_tree.bind("<Double-1>", self.on_double_click)
        
        detail_frame = ttk.Frame(self.file_tab)
        detail_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)
        
        ttk.Label(detail_frame, text="磁盘使用:").pack(side=tk.LEFT)
        self.disk_info = ttk.Label(detail_frame, text="--")
        self.disk_info.pack(side=tk.LEFT, padx=10)
    
    def create_trash_tab(self):
        toolbar = ttk.Frame(self.trash_tab)
        toolbar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        self.btn_restore = ttk.Button(toolbar, text="恢复", command=self.restore_item)
        self.btn_restore.pack(side=tk.LEFT, padx=2)
        
        self.btn_empty_trash = ttk.Button(toolbar, text="清空回收站", command=self.empty_trash)
        self.btn_empty_trash.pack(side=tk.LEFT, padx=2)
        
        self.trash_tree = ttk.Treeview(self.trash_tab, columns=("name", "type", "time", "inode"), show="headings")
        self.trash_tree.heading("name", text="文件名")
        self.trash_tree.heading("type", text="类型")
        self.trash_tree.heading("time", text="删除时间")
        self.trash_tree.heading("inode", text="Inode")
        
        self.trash_tree.column("name", width=200)
        self.trash_tree.column("type", width=80)
        self.trash_tree.column("time", width=150)
        self.trash_tree.column("inode", width=80)
        
        scrollbar = ttk.Scrollbar(self.trash_tab, orient=tk.VERTICAL, command=self.trash_tree.yview)
        self.trash_tree.configure(yscrollcommand=scrollbar.set)
        
        self.trash_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    
    def create_log_tab(self):
        toolbar = ttk.Frame(self.log_tab)
        toolbar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        self.btn_clear_log = ttk.Button(toolbar, text="清空日志", command=self.clear_log)
        self.btn_clear_log.pack(side=tk.LEFT, padx=2)
        
        self.log_text = tk.Text(self.log_tab, wrap=tk.WORD, state=tk.DISABLED)
        scrollbar = ttk.Scrollbar(self.log_tab, orient=tk.VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    
    def create_shell_tab(self):
        self.shell_text = tk.Text(self.shell_tab, wrap=tk.WORD, bg="black", fg="white", font=("Consolas", 10))
        scrollbar = ttk.Scrollbar(self.shell_tab, orient=tk.VERTICAL, command=self.shell_text.yview)
        self.shell_text.configure(yscrollcommand=scrollbar.set)
        
        self.shell_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        input_frame = ttk.Frame(self.shell_tab)
        input_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)
        
        ttk.Label(input_frame, text="命令:").pack(side=tk.LEFT)
        self.cmd_entry = ttk.Entry(input_frame)
        self.cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.cmd_entry.bind("<Return>", self.execute_command)
        
        self.shell_text.insert(tk.END, "Linux二级文件系统命令行\n")
        self.shell_text.insert(tk.END, "输入命令后按回车执行\n")
        self.shell_text.insert(tk.END, "输入 'help' 查看帮助\n")
        self.shell_text.insert(tk.END, "="*50 + "\n")
    
    def start_filesys(self):
        if os.path.exists("filesys"):
            self.process = subprocess.Popen(
                ["./filesys"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            threading.Thread(target=self.read_output, daemon=True).start()
            self.after_login("root", "root")
    
    def read_output(self):
        while self.process and self.process.poll() is None:
            try:
                line = self.process.stdout.readline()
                if line:
                    self.output_buffer.append(line)
                    self.update_shell_output(line)
            except:
                break
    
    def update_shell_output(self, line):
        self.shell_text.configure(state=tk.NORMAL)
        self.shell_text.insert(tk.END, line)
        self.shell_text.see(tk.END)
        self.shell_text.configure(state=tk.DISABLED)
    
    def send_command(self, cmd):
        if self.process:
            self.process.stdin.write(cmd + "\n")
            self.process.stdin.flush()
            time.sleep(0.2)
    
    def after_login(self, user, password):
        self.send_command(f"login {user}")
        time.sleep(0.1)
        self.send_command(password)
        time.sleep(0.3)
        self.current_user = user
        self.refresh_file_list()
        self.status_bar.config(text=f"已登录: {user}")
    
    def execute_command(self, event=None):
        cmd = self.cmd_entry.get()
        if cmd.strip():
            self.shell_text.configure(state=tk.NORMAL)
            self.shell_text.insert(tk.END, f"> {cmd}\n")
            self.shell_text.see(tk.END)
            self.shell_text.configure(state=tk.DISABLED)
            self.send_command(cmd)
            self.cmd_entry.delete(0, tk.END)
            
            if cmd.startswith("cd "):
                time.sleep(0.2)
                self.refresh_file_list()
            elif cmd == "trash":
                time.sleep(0.2)
                self.refresh_trash_list()
            elif cmd == "df":
                time.sleep(0.2)
                self.refresh_disk_info()
    
    def refresh_file_list(self):
        self.send_command("pwd")
        time.sleep(0.1)
        
        self.send_command("dir")
        time.sleep(0.2)
        
        self.parse_dir_output()
        self.refresh_disk_info()
    
    def parse_dir_output(self):
        for item in self.file_tree.get_children():
            self.file_tree.delete(item)
        
        if len(self.output_buffer) >= 2:
            lines = [line.strip() for line in self.output_buffer[-20:] if line.strip()]
            path_line = None
            dir_start = False
            
            for line in lines:
                if line.startswith("/") and len(line) < 50:
                    path_line = line
                if "文件名" in line and "Inode" in line:
                    dir_start = True
                    continue
                if dir_start and line.startswith("---"):
                    continue
                if dir_start and line and not line.startswith("共") and not line.startswith("("):
                    parts = line.split()
                    if len(parts) >= 5:
                        name = parts[0]
                        inode = parts[1]
                        perm = parts[2]
                        size = parts[3]
                        ftype = parts[4]
                        self.file_tree.insert("", tk.END, values=(inode, size, perm, ftype), text=name)
            
            if path_line:
                self.current_path = path_line
                self.path_var.set(path_line)
    
    def refresh_trash_list(self):
        self.send_command("trash")
        time.sleep(0.2)
        
        for item in self.trash_tree.get_children():
            self.trash_tree.delete(item)
        
        if len(self.output_buffer) >= 2:
            lines = [line.strip() for line in self.output_buffer[-20:] if line.strip()]
            trash_start = False
            
            for line in lines:
                if "文件名" in line and "类型" in line:
                    trash_start = True
                    continue
                if trash_start and line.startswith("---"):
                    continue
                if trash_start and line and not line.startswith("共") and not line.startswith("(空)"):
                    parts = line.split()
                    if len(parts) >= 4:
                        name = parts[0]
                        ftype = parts[1]
                        ftime = parts[2] + " " + parts[3] if len(parts) > 3 else parts[2]
                        inode = parts[-1]
                        self.trash_tree.insert("", tk.END, values=(name, ftype, ftime, inode))
    
    def refresh_disk_info(self):
        self.send_command("df")
        time.sleep(0.2)
        
        if len(self.output_buffer) >= 2:
            lines = [line.strip() for line in self.output_buffer[-10:] if line.strip()]
            for line in lines:
                if "总块数" in line:
                    parts = line.split()
                    total = parts[1]
                if "空闲块数" in line:
                    parts = line.split()
                    free = parts[1]
                    used = str(int(total) - int(free))
                    self.disk_info.config(text=f"总块: {total}, 已用: {used}, 空闲: {free}")
                    break
    
    def new_file(self):
        name = simpledialog.askstring("新建文件", "请输入文件名:")
        if name:
            self.send_command(f"create {name}")
            time.sleep(0.2)
            self.refresh_file_list()
    
    def new_dir(self):
        name = simpledialog.askstring("新建目录", "请输入目录名:")
        if name:
            self.send_command(f"mkdir {name}")
            time.sleep(0.2)
            self.refresh_file_list()
    
    def delete_item(self):
        selected = self.file_tree.selection()
        if selected:
            item = self.file_tree.item(selected[0])
            name = item["values"][0]
            if messagebox.askyesno("删除", f"确定要将 '{name}' 移入回收站吗?"):
                self.send_command(f"trash-move {name}")
                time.sleep(0.2)
                self.refresh_file_list()
    
    def rename_item(self):
        selected = self.file_tree.selection()
        if selected:
            item = self.file_tree.item(selected[0])
            old_name = item["values"][0]
            new_name = simpledialog.askstring("重命名", "请输入新名称:", initialvalue=old_name)
            if new_name and new_name != old_name:
                messagebox.showinfo("提示", "暂不支持直接重命名，请先创建新文件再复制内容")
    
    def open_file(self):
        selected = self.file_tree.selection()
        if selected:
            item = self.file_tree.item(selected[0])
            name = item["values"][0]
            ftype = item["values"][3]
            if ftype == "<DIR>":
                self.send_command(f"cd {name}")
                time.sleep(0.2)
                self.refresh_file_list()
            else:
                self.send_command(f"open {name} r")
                time.sleep(0.1)
                self.send_command(f"read 0 1000")
                time.sleep(0.2)
                
                content = ""
                for line in self.output_buffer[-10:]:
                    if "读取" in line or "EOF" in line:
                        continue
                    content += line
                
                if content:
                    self.show_file_content(name, content)
    
    def show_file_content(self, name, content):
        top = tk.Toplevel(self.root)
        top.title(f"文件内容 - {name}")
        top.geometry("600x400")
        
        text = tk.Text(top, wrap=tk.WORD)
        text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        text.insert(tk.END, content)
        text.configure(state=tk.DISABLED)
    
    def cd_up(self):
        self.send_command("cd ..")
        time.sleep(0.2)
        self.refresh_file_list()
    
    def on_double_click(self, event):
        self.open_file()
    
    def restore_item(self):
        selected = self.trash_tree.selection()
        if selected:
            item = self.trash_tree.item(selected[0])
            name = item["values"][0]
            if messagebox.askyesno("恢复", f"确定要恢复 '{name}' 吗?"):
                self.send_command(f"trash-restore {name}")
                time.sleep(0.2)
                self.refresh_trash_list()
                self.refresh_file_list()
    
    def empty_trash(self):
        if messagebox.askyesno("清空回收站", "确定要清空回收站吗?此操作不可恢复!"):
            self.send_command("trash-empty")
            time.sleep(0.2)
            self.refresh_trash_list()
    
    def clear_log(self):
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete(1.0, tk.END)
        self.log_text.configure(state=tk.DISABLED)

if __name__ == "__main__":
    from tkinter import simpledialog
    root = tk.Tk()
    app = FS_GUI(root)
    root.mainloop()