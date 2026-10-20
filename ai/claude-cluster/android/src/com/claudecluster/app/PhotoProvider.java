/*
 * claude-cluster for Android: the files of the camera. A tiny content provider (the platform has no FileProvider without androidx): the camera
 * app writes its photo / video in the cache folder of the app through a content:// URI, the page then uploads it. Nothing else is served.
 */

package com.claudecluster.app;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.database.MatrixCursor;

import java.io.File;
import java.io.FileNotFoundException;

public class PhotoProvider extends ContentProvider {
    static final String AUTHORITY = "com.claudecluster.app.photos";

    // the file of a new capture: <cache>/camera/<name>, its content:// URI given to the camera app
    static File fileFor(Context context, String name) {
        File folder = new File(context.getCacheDir(), "camera");
        folder.mkdirs();
        return new File(folder, name);
    }

    static Uri uriFor(String name) {
        return new Uri.Builder().scheme("content").authority(AUTHORITY).appendPath(name).build();
    }

    // only a plain name of the folder of the cameras: never a path out of it
    private File resolve(Uri uri) throws FileNotFoundException {
        String name = uri.getLastPathSegment();
        if (name == null || !name.matches("[A-Za-z0-9._-]+") || name.startsWith(".")) throw new FileNotFoundException("bad name");
        return fileFor(getContext(), name);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        File file = resolve(uri);
        int flags = mode.contains("w") ? ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_TRUNCATE : ParcelFileDescriptor.MODE_READ_ONLY;
        return ParcelFileDescriptor.open(file, flags);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
        // its name and size (the page asks them of a content:// URI)
        try {
            File file = resolve(uri);
            MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
            cursor.addRow(new Object[] {file.getName(), file.length()});
            return cursor;
        } catch (FileNotFoundException e) {
            return null;
        }
    }

    @Override
    public String getType(Uri uri) {
        String name = String.valueOf(uri.getLastPathSegment()).toLowerCase();
        return name.endsWith(".mp4") ? "video/mp4" : "image/jpeg";
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] args) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] args) {
        return 0;
    }
}
