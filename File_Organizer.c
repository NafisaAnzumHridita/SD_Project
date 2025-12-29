#include <gtk/gtk.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#endif

#define TRASH_FOLDER "Trash"
#define TAG_FILE "tags.txt"

enum
{
    COL_ICON,
    COL_NAME,
    COL_PATH,
    COL_IS_DIR,
    NUM_COLS
};

GtkWidget *window;
GtkWidget *tree;
GtkListStore *store;
GtkWidget *search_entry;
GtkWidget *filter_combo;
GtkWidget *path_label;
GtkWidget *view_btn;
GtkWidget *theme_btn;

char current_path[1024] = "";
char last_folder[1024] = "";
char search_text[256] = "";
char filter_ext[50] = "All";
gboolean dark_mode = FALSE;

void ensure_trash()
{
    struct stat st = {0};
    if (stat(TRASH_FOLDER, &st) == -1)
    {
#ifdef _WIN32
        _mkdir(TRASH_FOLDER);
#else
        mkdir(TRASH_FOLDER, 0777);
#endif
    }
}

void move_to_trash(const char *filepath)
{
    ensure_trash();
    char filename[1024];
    const char *base = strrchr(filepath, '/');
    if (base)
        strcpy(filename, base + 1);
    else
        strcpy(filename, filepath);
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/%s", TRASH_FOLDER, filename);
    rename(filepath, dest);
}

void restore_from_trash(const char *filename, const char *dest_folder)
{
    char src[1024], dest[1024];
    snprintf(src, sizeof(src), "%s/%s", TRASH_FOLDER, filename);
    snprintf(dest, sizeof(dest), "%s/%s", dest_folder, filename);
    rename(src, dest);
}

void save_tag(const char *filename, const char *tag)
{
    FILE *fp = fopen(TAG_FILE, "a");
    if (!fp)
        return;
    fprintf(fp, "%s | %s\n", filename, tag);
    fclose(fp);
}

void open_file(const char *path)
{
#ifdef _WIN32
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", path);
    system(cmd);
#else
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", path);
    system(cmd);
#endif
}

void auto_sort(const char *file)
{
    struct stat st;
    if (stat(file, &st) != 0)
        return;
    if (S_ISDIR(st.st_mode))
        return;
#ifdef _WIN32
    if (g_str_has_suffix(file, ".pdf"))
        _mkdir("Documents");
    else if (g_str_has_suffix(file, ".jpg") || g_str_has_suffix(file, ".png"))
        _mkdir("Images");
#else
    if (g_str_has_suffix(file, ".pdf"))
        mkdir("Documents", 0777);
    else if (g_str_has_suffix(file, ".jpg") || g_str_has_suffix(file, ".png"))
        mkdir("Images", 0777);
#endif
    if (g_str_has_suffix(file, ".pdf"))
    {
        char dest[1024];
        snprintf(dest, sizeof(dest), "Documents/%s", strrchr(file, '/') + 1);
        rename(file, dest);
    }
    else if (g_str_has_suffix(file, ".jpg") || g_str_has_suffix(file, ".png"))
    {
        char dest[1024];
        snprintf(dest, sizeof(dest), "Images/%s", strrchr(file, '/') + 1);
        rename(file, dest);
    }
}

void populate(const char *folder)
{
    gtk_list_store_clear(store);
    DIR *d = opendir(folder);
    if (!d)
        return;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") == 0)
            continue;
        if (search_text[0] && !g_strrstr(dir->d_name, search_text))
            continue;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", folder, dir->d_name);
        struct stat st;
        if (stat(fullpath, &st) != 0)
            continue;
        gboolean is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && strcmp(filter_ext, "All") != 0)
        {
            if (!g_str_has_suffix(dir->d_name, filter_ext))
                continue;
        }
        GtkTreeIter it;
        gtk_list_store_append(store, &it);
        gtk_list_store_set(store, &it,
                           COL_ICON, is_dir ? "folder" : "text-x-generic",
                           COL_NAME, dir->d_name,
                           COL_PATH, fullpath,
                           COL_IS_DIR, is_dir,
                           -1);
    }
    closedir(d);
}

void on_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path))
        return;
    gboolean is_dir;
    char *fullpath;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_PATH, &fullpath, COL_IS_DIR, &is_dir, -1);
    if (is_dir)
    {
        strcpy(current_path, fullpath);
        strcpy(last_folder, fullpath);
        gtk_label_set_text(GTK_LABEL(path_label), current_path);
        populate(current_path);
    }
    else
        open_file(fullpath);
    g_free(fullpath);
}

void on_search(GtkEditable *e, gpointer d)
{
    strcpy(search_text, gtk_entry_get_text(GTK_ENTRY(e)));
    populate(current_path);
}

void on_filter(GtkComboBoxText *c, gpointer d)
{
    const char *t = gtk_combo_box_text_get_active_text(c);
    if (t)
        strcpy(filter_ext, t);
    populate(current_path);
}

void on_folder_chosen(GtkFileChooserButton *b, gpointer d)
{
    char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(b));
    if (!p)
        return;
    strcpy(current_path, p);
    strcpy(last_folder, p);
    gtk_label_set_text(GTK_LABEL(path_label), current_path);
    populate(current_path);
    g_free(p);
}

void toggle_view(GtkButton *b, gpointer d)
{
    static gboolean headers = FALSE;
    headers = !headers;
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), headers);
}

void toggle_theme(GtkButton *b, gpointer d)
{
    GtkSettings *s = gtk_settings_get_default();
    dark_mode = !dark_mode;
    g_object_set(s, "gtk-application-prefer-dark-theme", dark_mode, NULL);
}

void batch_delete()
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    GList *rows = gtk_tree_selection_get_selected_rows(sel, NULL);
    for (GList *l = rows; l != NULL; l = l->next)
    {
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path))
        {
            gboolean is_dir;
            char *fullpath;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_PATH, &fullpath, COL_IS_DIR, &is_dir, -1);
            if (!is_dir)
                move_to_trash(fullpath);
            g_free(fullpath);
        }
        gtk_tree_path_free(path);
    }
    g_list_free(rows);
    populate(current_path);
}

void batch_restore()
{
    if (strlen(last_folder) == 0)
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                   GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                   "Open a folder first before restoring!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    GList *rows = gtk_tree_selection_get_selected_rows(sel, NULL);
    for (GList *l = rows; l != NULL; l = l->next)
    {
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path))
        {
            char *name;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_NAME, &name, -1);
            restore_from_trash(name, last_folder);
            g_free(name);
        }
        gtk_tree_path_free(path);
    }
    g_list_free(rows);
    populate(current_path);
}

gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
    if (e->keyval == GDK_KEY_Delete)
    {
        batch_delete();
        return TRUE;
    }
    return FALSE;
}

void auto_sort_current_folder(GtkButton *b, gpointer d)
{
    DIR *dptr = opendir(current_path);
    if (!dptr)
        return;
    struct dirent *entry;
    while ((entry = readdir(dptr)) != NULL)
    {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
        {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", current_path, entry->d_name);
            auto_sort(path);
        }
    }
    closedir(dptr);
    populate(current_path);
}

void view_trash(GtkButton *b, gpointer d)
{
    strcpy(current_path, TRASH_FOLDER);
    gtk_label_set_text(GTK_LABEL(path_label), "Trash");
    populate(TRASH_FOLDER);
}

void create_new_folder(GtkButton *b, gpointer d)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New Folder", GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Folder Name");
    GtkContainer *content = GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 5);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        const char *name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(name) > 0)
        {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", current_path, name);
#ifdef _WIN32
            _mkdir(path);
#else
            mkdir(path, 0777);
#endif
            populate(current_path);
        }
    }
    gtk_widget_destroy(dialog);
}

void create_new_file(GtkButton *b, gpointer d)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New File", GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "File Name");
    GtkContainer *content = GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 5);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        const char *name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(name) > 0)
        {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", current_path, name);
            FILE *fp = fopen(path, "w");
            if (fp)
                fclose(fp);
            populate(current_path);
        }
    }
    gtk_widget_destroy(dialog);
}

void compress_selected(GtkButton *b, gpointer d)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    GList *rows = gtk_tree_selection_get_selected_rows(sel, NULL);
    if (!rows)
        return;
    char cmd[2048] = {0};
#ifdef _WIN32
    strcpy(cmd, "powershell Compress-Archive -Path ");
#else
    strcpy(cmd, "zip -r compressed.zip ");
#endif
    for (GList *l = rows; l != NULL; l = l->next)
    {
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path))
        {
            char *name;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_NAME, &name, -1);
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", current_path, name);
            strcat(cmd, "\"");
            strcat(cmd, full);
            strcat(cmd, "\" ");
            g_free(name);
        }
        gtk_tree_path_free(path);
    }
    g_list_free(rows);
    system(cmd);
}

void preview_selected(GtkButton *b, gpointer d)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    GList *rows = gtk_tree_selection_get_selected_rows(sel, NULL);
    if (!rows)
        return;
    GtkTreePath *path = (GtkTreePath *)rows->data;
    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path))
        return;
    char *fullpath;
    gboolean is_dir;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_PATH, &fullpath, COL_IS_DIR, &is_dir, -1);
    if (!is_dir)
        open_file(fullpath);
    g_free(fullpath);
    g_list_free(rows);
}

void activate(GtkApplication *app, gpointer d)
{
    ensure_trash();
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "File Organizer");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    path_label = gtk_label_new("Select a folder...");
    gtk_box_pack_start(GTK_BOX(vbox), path_label, FALSE, FALSE, 5);
    GtkWidget *chooser = gtk_file_chooser_button_new("Select Folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    g_signal_connect(chooser, "file-set", G_CALLBACK(on_folder_chosen), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), chooser, FALSE, FALSE, 5);
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search...");
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), search_entry, FALSE, FALSE, 5);
    filter_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "All");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), ".pdf");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), ".docx");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), ".jpg");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), ".c");
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 0);
    g_signal_connect(filter_combo, "changed", G_CALLBACK(on_filter), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), filter_combo, FALSE, FALSE, 5);
    view_btn = gtk_button_new_with_label("Toggle View");
    g_signal_connect(view_btn, "clicked", G_CALLBACK(toggle_view), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), view_btn, FALSE, FALSE, 5);
    theme_btn = gtk_button_new_with_label("Light / Dark");
    g_signal_connect(theme_btn, "clicked", G_CALLBACK(toggle_theme), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), theme_btn, FALSE, FALSE, 5);
    GtkWidget *trash_btn = gtk_button_new_with_label("Move to Trash (Selected)");
    g_signal_connect(trash_btn, "clicked", G_CALLBACK(batch_delete), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), trash_btn, FALSE, FALSE, 5);
    GtkWidget *restore_btn = gtk_button_new_with_label("Restore from Trash (Selected)");
    g_signal_connect(restore_btn, "clicked", G_CALLBACK(batch_restore), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), restore_btn, FALSE, FALSE, 5);
    GtkWidget *view_trash_btn = gtk_button_new_with_label("View Trash");
    g_signal_connect(view_trash_btn, "clicked", G_CALLBACK(view_trash), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), view_trash_btn, FALSE, FALSE, 5);
    GtkWidget *sort_btn = gtk_button_new_with_label("Auto Sort Current Folder");
    g_signal_connect(sort_btn, "clicked", G_CALLBACK(auto_sort_current_folder), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), sort_btn, FALSE, FALSE, 5);
    GtkWidget *new_folder_btn = gtk_button_new_with_label("New Folder");
    g_signal_connect(new_folder_btn, "clicked", G_CALLBACK(create_new_folder), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), new_folder_btn, FALSE, FALSE, 5);
    GtkWidget *new_file_btn = gtk_button_new_with_label("New File");
    g_signal_connect(new_file_btn, "clicked", G_CALLBACK(create_new_file), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), new_file_btn, FALSE, FALSE, 5);
    GtkWidget *compress_btn = gtk_button_new_with_label("Compress Selected");
    g_signal_connect(compress_btn, "clicked", G_CALLBACK(compress_selected), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), compress_btn, FALSE, FALSE, 5);
    GtkWidget *preview_btn = gtk_button_new_with_label("Preview Selected");
    g_signal_connect(preview_btn, "clicked", G_CALLBACK(preview_selected), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), preview_btn, FALSE, FALSE, 5);
    store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
    g_signal_connect(tree, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), GTK_SELECTION_MULTIPLE);
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes("Name", r, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);
    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), NULL);
    gtk_widget_show_all(window);
}

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("com.file.organizer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
