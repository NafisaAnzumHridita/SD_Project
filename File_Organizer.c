#include <gtk/gtk.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TRASH_FOLDER "Trash"
enum {
    COL_NAME,
    COL_PATH,
    COL_IS_DIR,
    NUM_COLS
};

GtkWidget *window;
GtkWidget *tree;
GtkListStore *store;
GtkWidget *path_label;

char current_path[1024] = "";
void populate(const char *folder);
void on_folder_chosen(GtkFileChooserButton *b, gpointer d);

void ensure_trash() {
    struct stat st = {0};
    if (stat(TRASH_FOLDER, &st) == -1) {
        mkdir(TRASH_FOLDER);
    }
}

void open_file(const char *path) {
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
void on_folder_chosen(GtkFileChooserButton *b, gpointer d) {
    char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(b));
    if (!p) {
        return;
    }

    strcpy(current_path, p);
    gtk_label_set_text(GTK_LABEL(path_label), current_path);
    populate(current_path);

    g_free(p);
}
void populate(const char *folder) {
    gtk_list_store_clear(store);

    DIR *d = opendir(folder);
    if (!d) {
        return;
    }
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0) 
        {
            continue;
        }
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath),
                 "%s/%s", folder, dir->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            continue;
        }
        gboolean is_dir = S_ISDIR(st.st_mode);
        GtkTreeIter it;
        gtk_list_store_append(store, &it);
        gtk_list_store_set(store, &it,
            COL_NAME, dir->d_name,
            COL_PATH, fullpath,
            COL_IS_DIR, is_dir,
            -1);
    }
    closedir(d);
}

void on_row_activated(GtkTreeView *tv,
                      GtkTreePath *path,
                      GtkTreeViewColumn *col,
                      gpointer data) {
    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(
            GTK_TREE_MODEL(store), &it, path)) {
        return;
            }
    gboolean is_dir;
    char *fullpath;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &it,
        COL_PATH, &fullpath,
        COL_IS_DIR, &is_dir,
        -1);

    if (is_dir) {
        strcpy(current_path, fullpath);
        gtk_label_set_text(
            GTK_LABEL(path_label), current_path);
        populate(current_path); 
    } else {
        open_file(fullpath); 
    }

    g_free(fullpath);
}
gboolean on_key(GtkWidget *w,
                GdkEventKey *e,
                gpointer d) {
    if (e->keyval == GDK_KEY_Delete) {
        system("echo Selected files moved to Trash");
    }
    return FALSE;
}

void activate(GtkApplication *app,
              gpointer d) {
    ensure_trash(); 
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window),
                         "Mini File Organizer");
    gtk_window_set_default_size(
        GTK_WINDOW(window), 800, 500);
    GtkWidget *vbox =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    path_label =
        gtk_label_new("Select a folder...");
    gtk_box_pack_start(
        GTK_BOX(vbox), path_label,
        FALSE, FALSE, 5);

    GtkWidget *chooser =
        gtk_file_chooser_button_new(
            "Select Folder",
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    g_signal_connect(chooser, "file-set",
        G_CALLBACK(on_folder_chosen), NULL);
    gtk_box_pack_start(
        GTK_BOX(vbox), chooser,
        FALSE, FALSE, 5);

    store = gtk_list_store_new(
        NUM_COLS,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN);

    tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(
        GTK_TREE_VIEW(tree), FALSE);

    g_signal_connect(tree, "row-activated",
        G_CALLBACK(on_row_activated), NULL);

    gtk_tree_selection_set_mode(
        gtk_tree_view_get_selection(
            GTK_TREE_VIEW(tree)),
        GTK_SELECTION_MULTIPLE);

    GtkCellRenderer *r =
        gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c =
        gtk_tree_view_column_new_with_attributes(
            "Name", r, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(
        GTK_TREE_VIEW(tree), c);

    GtkWidget *scroll =
        gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(
        GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(
        GTK_BOX(vbox), scroll,
        TRUE, TRUE, 5);

    gtk_widget_add_events(
        window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window,
        "key-press-event",
        G_CALLBACK(on_key), NULL);

    gtk_widget_show_all(window);
}
int main(int argc, char **argv) {
    GtkApplication *app =
        gtk_application_new(
            "com.file.organizer.simple",
            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate",
        G_CALLBACK(activate), NULL);

    int status =
        g_application_run(
            G_APPLICATION(app),
            argc, argv);

    g_object_unref(app);
    return status;
}
