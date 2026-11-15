package com.mh4u.port;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
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
 * A normal Android text field over the game, for everything the settings ask the player to type (a nickname, an address, a
 * password, a hub server, a key...): the phone's own keyboard, and its clipboard (a long press pastes), as any app's. It
 * replaces the app's own keys. The native side (app_native/settings_panel.cpp, through GameActivity's static methods) polls
 * state() and result(), as it does the chat socket's: 0 none, 1 open, 2 accepted (result() has the text), 3 cancelled.
 */
final class TextFieldDialog {
    static final int KIND_ANY = 0, KIND_NICK = 1, KIND_ADDRESS = 2, KIND_DIGITS = 3, KIND_SECRET = 4, KIND_CHAT = 5;

    static volatile int state = 0;
    static volatile String result = "";
    private static AlertDialog dialog;

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
                LinearLayout box = new LinearLayout(activity);
                box.setOrientation(LinearLayout.VERTICAL);
                int pad = dp(activity, 20);
                box.setPadding(pad, dp(activity, 8), pad, 0);
                if (hint != null && !hint.isEmpty()) {
                    TextView help = new TextView(activity);
                    help.setText(hint);
                    help.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
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
                box.addView(field, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                                                 LinearLayout.LayoutParams.WRAP_CONTENT));
                if (error != null && !error.isEmpty()) {
                    TextView problem = new TextView(activity);
                    problem.setText(error);
                    problem.setTextColor(Color.rgb(0xB0, 0x20, 0x20));
                    problem.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                    problem.setPadding(0, dp(activity, 6), 0, 0);
                    box.addView(problem);
                }
                final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
                builder.setTitle(title);
                builder.setView(box);
                builder.setPositiveButton(android.R.string.ok, null);  // (set below: so that a tap does not close it twice)
                builder.setNegativeButton(android.R.string.cancel, null);
                final AlertDialog made = builder.create();
                made.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                                                  | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
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
                made.setOnShowListener(new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface d) {
                        made.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                accept.run();
                            }
                        });
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

    /** Takes the answer: back to none. */
    static void reset() {
        state = 0;
        result = "";
    }
}
