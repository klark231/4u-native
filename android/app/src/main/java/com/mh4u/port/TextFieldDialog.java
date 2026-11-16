package com.mh4u.port;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.Window;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Spanned;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A text field over the game, in the menu's frame and colours, at the top of the screen so the keyboard never covers it:
 * the title, what to type (two lines at most), and the field with OK and Cancel beside it. A normal Android field, for everything the settings ask the player to type (a nickname, an address, a
 * password, a hub server, a key...): the phone's own keyboard, and its clipboard (a long press pastes), as any app's. It
 * replaces the app's own keys. The native side (app_native/settings_panel.cpp, through GameActivity's static methods) polls
 * state() and result(), as it does the chat socket's: 0 none, 1 open, 2 accepted (result() has the text), 3 cancelled.
 */
final class TextFieldDialog {
    static final int KIND_ANY = 0, KIND_NICK = 1, KIND_ADDRESS = 2, KIND_DIGITS = 3, KIND_SECRET = 4, KIND_CHAT = 5;

    static volatile int state = 0;
    static volatile String result = "";
    private static Dialog dialog;

    private static int dp(Activity a, float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, a.getResources().getDisplayMetrics()));
    }

    /** The characters a kind lets in: a nickname as a room allows it, an address, a port. */
    private static boolean allowed(int kind, char c) {
        boolean alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        switch (kind) {
            case KIND_NICK: return alnum || c == ' ' || c == '.' || c == '_' || c == '-';
            case KIND_ADDRESS: return alnum || c == '.' || c == '-' || c == ':';
            case KIND_DIGITS: return c >= '0' && c <= '9';
            default: return c >= 0x20 && c != 0x7f;
        }
    }

    /** Shows the dialog (any thread). A dialog already up is replaced. */
    static void show(final Activity activity, final String title, final String hint, final String initial, final int kind,
                     final int maxLen, final String error) {
        state = 1;
        result = "";
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (dialog != null) {
                    try {
                        dialog.setOnDismissListener(null);
                        dialog.dismiss();
                    } catch (RuntimeException ignored) {
                    }
                    dialog = null;
                }
                final Dialog made = new Dialog(activity);
                made.requestWindowFeature(Window.FEATURE_NO_TITLE);
                LinearLayout box = new LinearLayout(activity);
                box.setOrientation(LinearLayout.VERTICAL);
                box.setBackground(Look.frame(activity, Look.WINE));
                int pad = dp(activity, 16);
                box.setPadding(pad, dp(activity, 10), pad, dp(activity, 12));
                TextView head = new TextView(activity);
                head.setText(title);
                head.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
                head.setTextColor(Look.GOLD);
                head.setTypeface(Typeface.SERIF, Typeface.BOLD);
                box.addView(head);
                View rule = new View(activity);
                rule.setBackgroundColor(Color.rgb(176, 34, 30));
                LinearLayout.LayoutParams rl = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(activity, 2));
                rl.setMargins(0, dp(activity, 6), 0, dp(activity, 6));
                box.addView(rule, rl);
                if (hint != null && !hint.isEmpty()) {
                    TextView help = new TextView(activity);
                    help.setText(hint);
                    help.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                    help.setTextColor(Color.rgb(196, 178, 150));
                    help.setMaxLines(2);
                    help.setEllipsize(android.text.TextUtils.TruncateAt.END);
                    help.setPadding(0, 0, 0, dp(activity, 8));
                    box.addView(help);
                }
                final EditText field = new EditText(activity);
                field.setSingleLine(true);
                field.setText(initial == null ? "" : initial);
                field.setSelection(field.getText().length());
                int type = InputType.TYPE_CLASS_TEXT;
                if (kind == KIND_DIGITS) type = InputType.TYPE_CLASS_NUMBER;
                else if (kind == KIND_SECRET) type |= InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
                else if (kind == KIND_NICK) type |= InputType.TYPE_TEXT_VARIATION_PERSON_NAME | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                else if (kind == KIND_ADDRESS) type |= InputType.TYPE_TEXT_VARIATION_URI | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                else if (kind == KIND_CHAT) type |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
                else type |= InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                field.setInputType(type);
                field.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
                field.setFilters(new InputFilter[] {
                    new InputFilter() {  // (what is typed or pasted: only the characters the kind lets in)
                        @Override
                        public CharSequence filter(CharSequence src, int start, int end, Spanned dst, int ds, int de) {
                            StringBuilder out = null;
                            for (int i = start; i < end; i++) {
                                char c = src.charAt(i);
                                if (c == '\n' || c == '\r' || c == '\t') c = ' ';  // (a pasted line break: a space)
                                if (!allowed(kind, c)) {
                                    if (out == null) out = new StringBuilder(src.subSequence(start, i));
                                } else if (out != null) {
                                    out.append(c);
                                } else if (c != src.charAt(i)) {
                                    out = new StringBuilder(src.subSequence(start, i)).append(c);
                                }
                            }
                            return out;  // (null: as it came)
                        }
                    },
                    new InputFilter.LengthFilter(Math.max(1, maxLen))
                });
                field.setTextColor(Look.CREAM);
                field.setHintTextColor(Color.rgb(150, 128, 104));
                field.setTextSize(TypedValue.COMPLEX_UNIT_SP, 17);
                field.setBackground(Look.frame(activity, Color.rgb(26, 12, 9)));
                field.setPadding(dp(activity, 14), dp(activity, 8), dp(activity, 14), dp(activity, 8));
                // the field, OK and Cancel on one row: all of it stays above the keyboard
                LinearLayout row = new LinearLayout(activity);
                row.setOrientation(LinearLayout.HORIZONTAL);
                row.setGravity(Gravity.CENTER_VERTICAL);
                row.addView(field, new LinearLayout.LayoutParams(0, dp(activity, 48), 1f));
                final TextView ok = choice(activity, activity.getString(android.R.string.ok), true);
                final TextView cancel = choice(activity, activity.getString(android.R.string.cancel), false);
                LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(activity, 48));
                bl.leftMargin = dp(activity, 8);
                row.addView(ok, bl);
                row.addView(cancel, new LinearLayout.LayoutParams(bl));
                box.addView(row, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                                               LinearLayout.LayoutParams.WRAP_CONTENT));
                if (error != null && !error.isEmpty()) {
                    TextView problem = new TextView(activity);
                    problem.setText(error);
                    problem.setTextColor(Look.ALERT);
                    problem.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                    problem.setPadding(0, dp(activity, 6), 0, 0);
                    box.addView(problem);
                }
                made.setContentView(box);
                Window win = made.getWindow();
                win.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
                win.setGravity(Gravity.TOP | Gravity.CENTER_HORIZONTAL);
                int width = activity.getResources().getDisplayMetrics().widthPixels;
                win.setLayout(Math.min(Math.round(width * 0.9f), dp(activity, 900)), ViewGroup.LayoutParams.WRAP_CONTENT);
                win.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                                     | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
                final Runnable accept = new Runnable() {
                    @Override
                    public void run() {
                        result = field.getText().toString();
                        state = 2;
                        made.setOnDismissListener(null);
                        made.dismiss();
                        dialog = null;
                    }
                };
                ok.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        accept.run();
                    }
                });
                cancel.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        made.dismiss();  // (as Back: the dismiss listener says cancelled)
                    }
                });
                made.setOnShowListener(new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface d) {
                        field.requestFocus();
                    }
                });
                field.setOnEditorActionListener(new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        if (actionId == EditorInfo.IME_ACTION_DONE || actionId == EditorInfo.IME_ACTION_SEND
                            || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                            accept.run();
                            return true;
                        }
                        return false;
                    }
                });
                made.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface d) {  // (Cancel, Back, a tap away from it)
                        if (state == 1) state = 3;
                        dialog = null;
                    }
                });
                dialog = made;
                made.show();
            }
        });
    }

    // A button of the dialog, as the menu's: framed, gold letters on bronze for OK, cream on the panel for Cancel
    private static TextView choice(Activity a, String label, boolean main) {
        TextView b = new TextView(a);
        b.setText(label);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        b.setTextColor(main ? Look.GOLD : Look.CREAM);
        if (main) b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setGravity(Gravity.CENTER);
        b.setBackground(Look.frame(a, main ? Look.BRONZE : Look.PANEL));
        b.setPadding(dp(a, 20), 0, dp(a, 20), 0);
        return b;
    }

    /** Takes the answer: back to none. */
    static void reset() {
        state = 0;
        result = "";
    }
}
