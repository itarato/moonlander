package com.example.moonlandingcontroller

import android.annotation.SuppressLint
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.MotionEvent
import android.widget.Button
import android.widget.Toast
import java.net.InetAddress
import java.net.Socket
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val engage_button: Button = findViewById(R.id.engine_button);
        val ctx = this
        engage_button.setOnTouchListener { v, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                thread {
                    val addr = InetAddress.getByAddress(byteArrayOf(192.toByte(), 168.toByte(), 0.toByte(), 109.toByte()))
                    val socket = Socket(addr, 8888)
                    socket.getOutputStream().write(1)
                }
                true
            } else if (event.action == MotionEvent.ACTION_UP) {
                thread {
                    val addr = InetAddress.getByAddress(byteArrayOf(192.toByte(), 168.toByte(), 0.toByte(), 109.toByte()))
                    val socket = Socket(addr, 8888)
                    socket.getOutputStream().write(16)
                }
                true
            } else {
                false
            }
        }
    }
}
