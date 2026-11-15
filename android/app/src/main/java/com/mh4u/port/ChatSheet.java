package com.mh4u.port;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The chat button's dialog: a bottom sheet over the game with the chat's lines and a normal text field with Send, as
 * AzaharPlus's chat dialog is (its phone keyboard, its clipboard). The native side (app_native/settings_panel.cpp) keeps
 * the chat (app_native/chat/chat.h) and polls this class through GameActivity's static methods: update() gives the sheet
 * what to show, takeSend() / takeNext() / takeJoin() give back what the player did. A line is "kind US who US text US serial",
 * the lines apart by RS: kind 0 a note, 1 your own, 2 the room's, 3 the online chat's, 4 a friend's, 5 the hub's; serial is not empty on a
 * line with a room shared in it.
 */
final class ChatSheet {
    static volatile boolean open = false;
    private static Dialog dialog;
    private static LinearLayout list;
    private static ScrollView scroller;
    private static TextView channelButton, problem;
    private static EditText field;

    private static String pendingSend = null;
    private static boolean pendingNext = false;
    private static String pendingJoin = null;

    private static int dp(Activity a, float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, a.getResources().getDisplayMetrics()));
    }

    static synchronized String takeSend() {
        String t = pendingSend;
        pendingSend = null;
        return t == null ? "" : t;
    }

    static synchronized boolean takeNext() {
        boolean n = pendingNext;
        pendingNext = false;
        return n;
    }

    static synchronized String takeJoin() {
        String j = pendingJoin;
        pendingJoin = null;
        return j == null ? "" : j;
    }

    /** Shows the sheet (any thread): opacity is the sheet's background, 20 to 100 %. */
    static void show(final Activity activity, final int opacity, final String title, final String hint, final String send) {
        open = true;
        synchronized (ChatSheet.class) {
            pendingSend = null;
            pendingNext = false;
            pendingJoin = null;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                dismissExisting();
                final Dialog d = new Dialog(activity);
                d.requestWindowFeature(Window.FEATURE_NO_TITLE);
                int alpha = Math.max(51, Math.min(255, Math.round(Math.max(20, Math.min(100, opacity)) * 2.55f)));
                GradientDrawable back = new GradientDrawable();
                back.setColor(Color.argb(alpha, 44, 22, 20));  // (the menu's wine, as see-through as Chat dialog opacity says)
                back.setStroke(dp(activity, 1), Look.EDGE);
                float r = dp(activity, 20);
                back.setCornerRadii(new float[] {r, r, r, r, 0, 0, 0, 0});

                LinearLayout root = new LinearLayout(activity);
                root.setOrientation(LinearLayout.VERTICAL);
                root.setBackground(back);
                int pad = dp(activity, 16);
                root.setPadding(pad, dp(activity, 10), pad, dp(activity, 10));

                // the top: the chat's name (a tap goes to the next chat) over a centred title
                LinearLayout top = new LinearLayout(activity);
                top.setOrientation(LinearLayout.HORIZONTAL);
                top.setGravity(Gravity.CENTER_VERTICAL);
                channelButton = new TextView(activity);
                channelButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                channelButton.setTextColor(Look.GOLD);
                channelButton.setTypeface(Typeface.DEFAULT_BOLD);
                channelButton.setPadding(dp(activity, 10), dp(activity, 6), dp(activity, 10), dp(activity, 6));
                GradientDrawable chip = new GradientDrawable();
                chip.setColor(Look.PANEL);
                chip.setStroke(dp(activity, 1), Look.EDGE);
                chip.setCornerRadius(dp(activity, 14));
                channelButton.setBackground(chip);
                channelButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        synchronized (ChatSheet.class) {
                            pendingNext = true;
                        }
                    }
                });
                TextView heading = new TextView(activity);
                heading.setText(title);
                heading.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
                heading.setTextColor(Look.GOLD);
                heading.setTypeface(Typeface.SERIF, Typeface.BOLD);
                heading.setGravity(Gravity.CENTER);
                TextView shut = new TextView(activity);
                shut.setText("\u2715");
                shut.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
                shut.setTextColor(Look.CREAM);
                shut.setPadding(dp(activity, 10), dp(activity, 4), dp(activity, 6), dp(activity, 4));
                shut.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        close(true);
                    }
                });
                top.addView(channelButton, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                top.addView(heading, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
                top.addView(shut, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                root.addView(top, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                View rule = new View(activity);  // (the menu's red rule under its title)
                rule.setBackgroundColor(Color.rgb(176, 34, 30));
                LinearLayout.LayoutParams ruleSize = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(activity, 2));
                ruleSize.setMargins(0, dp(activity, 6), 0, dp(activity, 4));
                root.addView(rule, ruleSize);

                // the lines
                scroller = new ScrollView(activity);
                list = new LinearLayout(activity);
                list.setOrientation(LinearLayout.VERTICAL);
                scroller.addView(list, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                root.addView(scroller, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

                problem = new TextView(activity);
                problem.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
                problem.setTextColor(Look.ALERT);
                problem.setVisibility(View.GONE);
                root.addView(problem, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

                // the field and Send
                LinearLayout bottom = new LinearLayout(activity);
                bottom.setOrientation(LinearLayout.HORIZONTAL);
                bottom.setGravity(Gravity.CENTER_VERTICAL);
                field = new EditText(activity);
                field.setHint(hint);
                field.setSingleLine(true);
                field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
                field.setImeOptions(EditorInfo.IME_ACTION_SEND | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
                field.setTextColor(Look.CREAM);
                field.setHintTextColor(Color.rgb(150, 128, 104));
                GradientDrawable outline = new GradientDrawable();
                outline.setColor(Color.argb(200, 26, 12, 9));
                outline.setStroke(dp(activity, 1), Look.EDGE);
                outline.setCornerRadius(dp(activity, 6));
                field.setBackground(outline);
                field.setPadding(dp(activity, 12), dp(activity, 12), dp(activity, 12), dp(activity, 12));
                final Runnable sendNow = new Runnable() {
                    @Override
                    public void run() {
                        String text = field.getText().toString().trim();
                        if (text.isEmpty()) return;
                        synchronized (ChatSheet.class) {
                            pendingSend = text;
                        }
                        field.setText("");
                    }
                };
                field.setOnEditorActionListener(new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        if (actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_ACTION_DONE
                            || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                            sendNow.run();
                            return true;
                        }
                        return false;
                    }
                });
                ImageButton sendButton = new ImageButton(activity);
                sendButton.setImageResource(android.R.drawable.ic_menu_send);
                sendButton.setContentDescription(send);
                sendButton.setBackgroundColor(Color.TRANSPARENT);
                sendButton.setColorFilter(Look.GOLD);
                sendButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        sendNow.run();
                    }
                });
                LinearLayout.LayoutParams fp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
                bottom.addView(field, fp);
                bottom.addView(sendButton, new LinearLayout.LayoutParams(dp(activity, 48), dp(activity, 48)));
                LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                bp.topMargin = dp(activity, 8);
                root.addView(bottom, bp);

                d.setContentView(root);
                Window w = d.getWindow();
                w.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
                w.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);  // (the game stays seen above it)
                w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                int screen = activity.getResources().getDisplayMetrics().heightPixels;
                int width = Math.min(activity.getResources().getDisplayMetrics().widthPixels, dp(activity, 640));
                w.setLayout(width, Math.round(screen * 0.62f));
                w.setGravity(Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
                d.setCanceledOnTouchOutside(true);
                d.setOnDismissListener(new android.content.DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(android.content.DialogInterface x) {
                        open = false;
                        dialog = null;
                    }
                });
                dialog = d;
                d.show();
            }
        });
    }

    /** What the sheet shows: the chat's name, and the lines (see the class comment). */
    static void update(final Activity activity, final String channel, final String lines, final String hint) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (dialog == null || list == null) return;
                channelButton.setText(channel);
                field.setHint(hint);
                boolean atEnd = scroller.getChildAt(0).getBottom() - (scroller.getHeight() + scroller.getScrollY()) < dp(activity, 40);
                list.removeAllViews();
                if (lines != null && !lines.isEmpty()) {
                    for (String record : lines.split("\u001e", -1)) {
                        String[] f = record.split("\u001f", -1);
                        if (f.length < 3) continue;
                        list.addView(lineView(activity, f[0], f[1], f[2], f.length > 3 ? f[3] : ""));
                    }
                }
                if (atEnd) scroller.post(new Runnable() {
                    @Override
                    public void run() {
                        scroller.fullScroll(View.FOCUS_DOWN);
                    }
                });
            }
        });
    }

    private static View lineView(final Activity activity, String kind, String who, String text, final String serial) {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(dp(activity, 4), dp(activity, 6), dp(activity, 4), dp(activity, 6));
        if (!kind.equals("0") && !who.isEmpty()) {
            TextView name = new TextView(activity);
            name.setText(who);
            name.setTypeface(Typeface.DEFAULT_BOLD);
            name.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            // (lighter colours that read on the wine: you gold, the room green, a friend pink, the hub orange)
            name.setTextColor(kind.equals("1") ? Look.GOLD
                              : kind.equals("2") ? Color.rgb(150, 206, 116)
                              : kind.equals("4") ? Color.rgb(236, 150, 196)
                              : kind.equals("5") ? Color.rgb(240, 166, 84) : Look.CREAM);
            row.addView(name);
        }
        TextView body = new TextView(activity);
        body.setText(text);
        body.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        body.setTextColor(kind.equals("0") ? Color.rgb(176, 156, 132) : Look.CREAM);
        row.addView(body);
        if (!serial.isEmpty()) {  // (a room shared in the line: a tap joins it)
            row.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    synchronized (ChatSheet.class) {
                        pendingJoin = serial;
                    }
                }
            });
            body.setTextColor(Look.GOLD);
            body.setPaintFlags(body.getPaintFlags() | android.graphics.Paint.UNDERLINE_TEXT_FLAG);
        }
        return row;
    }

    /** A reason a line was not sent (empty: none), under the lines. */
    static void error(final Activity activity, final String text) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (problem == null) return;
                problem.setText(text);
                problem.setVisibility(text == null || text.isEmpty() ? View.GONE : View.VISIBLE);
            }
        });
    }

    /** A sheet that is up already goes (UI thread), without saying the chat is closed: a new one comes. */
    private static void dismissExisting() {
        final Dialog d = dialog;
        dialog = null;
        if (d == null) return;
        d.setOnDismissListener(null);
        try {
            d.dismiss();
        } catch (RuntimeException ignored) {
        }
    }

    /** Closes the sheet (any thread). */
    static void close(final boolean fromPlayer) {
        open = false;
        final Dialog d = dialog;
        if (d == null) return;
        dialog = null;
        Activity a = (Activity) d.getContext();
        a.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    d.dismiss();
                } catch (RuntimeException ignored) {
                }
            }
        });
    }
}
